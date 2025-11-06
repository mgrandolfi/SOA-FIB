/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include <errno.h>


/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %edx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %ebx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %ebx; \
      pushl %ecx; \
      pushl %edx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es;

#define RESTORE_ALL \
      popl %edx; \
      popl %ecx; \
      popl %ebx; \
      popl %esi; \
      popl %edi; \
      popl %ebp; \
      popl %eax; \
      popl %ds; \
      popl %es; \
      popl %fs; \
      popl %gs;

#define EOI \
      movb $0x20, %al; \
      outb %al, $0x20;

ENTRY(keyboard_handler)
      SAVE_ALL
      EOI
      call keyboard_routine
      RESTORE_ALL
      iret

ENTRY(clock_handler)
      SAVE_ALL
      EOI
      call clock_routine
      RESTORE_ALL
      iret

ENTRY(pf_handler)
      movl 0x04(%esp), %eax
      pushl %eax
      call pf_routine

ENTRY(system_call_handler)
      SAVE_ALL                            // Save the current context
      cmpl $0, %EAX                       // Is syscall number negative?
      jl err                              // If it is, jump to return an error
      cmpl $MAX_SYSCALL, %EAX             // Is syscall greater than MAX_SYSCALL (4)?
      jg err                              // If it is, jump to return an error
      call *sys_call_table(, %EAX, 0x04)  // Call the corresponding service routine
      jmp fin                             // Finish
   err:
      movl $-ENOSYS, %EAX                 // call sys_ni_syscall? Move to EAX the ENOSYS error
   fin:
      movl %EAX, 0x18(%esp)               // Change the EAX value in the stack
      RESTORE_ALL                         // Restore the context
      iret

ENTRY(syscall_handler_sysenter)
      push $__USER_DS
      push %EBP                           // User stack address
      pushfl
      push $__USER_CS
      push 4(%EBP)                        // User return address
      SAVE_ALL
      cmpl $0, %EAX
      jl sysenter_err
      cmpl $MAX_SYSCALL, %EAX
      jg sysenter_err
      call *sys_call_table(, %EAX, 0x04)
      jmp sysenter_fin
   sysenter_err:
      movl $-ENOSYS, %EAX
   sysenter_fin:
      movl %EAX, 0x18(%ESP)
      RESTORE_ALL
      movl (%ESP), %EDX                   // Return address
      movl 12(%ESP), %ECX                 // User stack address
      sti                                 // Enable interrupts again
      sysexit

ENTRY(writeMSR)
      push %ebp
      movl %esp, %ebp

      movl $0, %edx
      movl 8(%ebp), %ecx
      movl 12(%ebp), %eax
      wrmsr

      popl %ebp;
      ret

ENTRY(task_switch)
    pushl %ebp
    movl %esp, %ebp

    pushl %edi
    pushl %esi
    pushl %ebx

    pushl 8(%ebp)
    call inner_task_switch
    addl $4, %esp

    popl %ebx
    popl %esi
    popl %edi
    movl %ebp, %esp
    popl %ebp
    ret

ENTRY(switch_context)
    movl 4(%esp), %eax
    movl %ebp, (%eax)
    movl 8(%esp), %esp
    
    popl %ebp
    ret