# 0 "entry.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "entry.S"




# 1 "include/asm.h" 1
# 6 "entry.S" 2
# 1 "include/segment.h" 1
# 7 "entry.S" 2
# 1 "include/errno.h" 1
# 8 "entry.S" 2
# 72 "entry.S"
.globl keyboard_handler; .type keyboard_handler, @function; .align 0; keyboard_handler:
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es;
      movb $0x20, %al; outb %al, $0x20;
      call keyboard_routine
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl clock_handler; .type clock_handler, @function; .align 0; clock_handler:
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es;
      movb $0x20, %al; outb %al, $0x20;
      call clock_routine
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl pf_handler; .type pf_handler, @function; .align 0; pf_handler:
      movl 0x04(%esp), %eax
      pushl %eax
      call pf_routine

.globl system_call_handler; .type system_call_handler, @function; .align 0; system_call_handler:
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es;
      cmpl $0, %EAX
      jl err
      cmpl $MAX_SYSCALL, %EAX
      jg err
      call *sys_call_table(, %EAX, 0x04)
      jmp fin
   err:
      movl $-38, %EAX
   fin:
      movl %EAX, 0x18(%esp)
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl syscall_handler_sysenter; .type syscall_handler_sysenter, @function; .align 0; syscall_handler_sysenter:
      push $0x2B
      push %EBP
      pushfl
      push $0x23
      push 4(%EBP)
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es;
      cmpl $0, %EAX
      jl sysenter_err
      cmpl $MAX_SYSCALL, %EAX
      jg sysenter_err
      call *sys_call_table(, %EAX, 0x04)
      jmp sysenter_fin
   sysenter_err:
      movl $-38, %EAX
   sysenter_fin:
      movl %EAX, 0x18(%ESP)
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      movl (%ESP), %EDX
      movl 12(%ESP), %ECX
      sti
      sysexit

.globl writeMSR; .type writeMSR, @function; .align 0; writeMSR:
      push %ebp
      movl %esp, %ebp

      movl $0, %edx
      movl 8(%ebp), %ecx
      movl 12(%ebp), %eax
      wrmsr

      popl %ebp;
      ret
