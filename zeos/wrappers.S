#include <asm.h>
#include <segment.h>

ENTRY(gettime)
    movl $10, %EAX;        // sys_gettime pos 10 a la sys_call_table
    int  $0x80;            // trap -> kernel
    call system_call_handler;
    ret

ENTRY(write)
    // The parameters of the stack must be copied to the registers EDX, ECX, EBX, ESI, EDI, EBP.
    push %ebp;
    movl %esp, %ebp;
    movl 8(%ebp), %edx;
    movl 12(%ebp), %ecx;
    movl 16(%ebp), %ebx;
    movl 4, %eax;
    int 0x80;
    call system_call_handler;
    mov %eax, 24(%esp);
    popl %ebp;
    ret