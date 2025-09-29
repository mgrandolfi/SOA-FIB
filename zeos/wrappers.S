#include <asm.h>
#include <segment.h>

ENTRY(gettime)
    movl $10, %EAX        /* syscall id 10 = gettime */
    int  $0x80            /* trap into the kernel */
    ret

ENTRY(write)
    /* The parameters of the stack must be copied to the registers EDX, ECX, EBX, ESI, EDI, EBP.*/
    movl  8(%ESP), %EDX
    movl 12(%ESP), %ECX
    movl 16(%ESP), %EBX

    movl $4, %EAX           //sys_write a la sys_call_table es la posicio 4
    int  $0x80              //cridem entrada interrupcio syscalls

    /* if (eax < 0) { errno = -eax; return -1; } */
    cmpl $0, %EAX
    jge 1f
    negl %EAX
    movl %EAX, errno
    movl $-1, %EAX
1:  ret