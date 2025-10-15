#include <asm.h>

ENTRY(gettime)
    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %esi
    pushl %edi

    pushl %ecx                      // guardem ECX
    pushl %edx                      // guardem EDX
    pushl $after_sysenter_gettime   // user return EIP per al sysexit
    pushl %ebp                      // fake frame link
    movl %esp, %ebp                 // EBP ara apunta al frame del kernel

    movl $10, %eax
    sysenter                        // trap -> kernel

after_sysenter_gettime:
    popl %ebp       // desfem frame
    addl $4, %esp
    popl %edx
    popl %ecx

    popl %edi       // restaurem valors
    popl %esi
    popl %ebx

    movl %ebp, %esp // no fem error management a gettime
    popl %ebp
    ret

ENTRY(write)
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp),  %edx    // fd   -> EDX
    movl 12(%ebp), %ecx    // buf  -> ECX
    movl 16(%ebp), %ebx    // size -> EBX

    pushl %ebx
    pushl %esi
    pushl %edi

    pushl %ecx                      // guardem ECX
    pushl %edx                      // guardem EDX
    pushl $after_sysenter_write     // user return EI
    pushl %ebp                      // creem fake frame link
    movl %esp, %ebp                

    movl $4, %eax
    sysenter

after_sysenter_write:
    popl %ebp       // desfem frame
    addl $4, %esp
    popl %edx
    popl %ecx

    popl %edi       //restaurem els valors
    popl %esi
    popl %ebx

    cmpl $0, %eax   // errno management
    jge write_return
    negl %eax
    movl %eax, errno
    movl $-1, %eax
    
write_return:
    movl %ebp, %esp
    popl %ebp
    ret