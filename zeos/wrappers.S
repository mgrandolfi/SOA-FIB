#include <asm.h>
#include <segment.h>

ENTRY(gettime)
    movl $10, %EAX        /* syscall id 10 = gettime */
    int  $0x80            /* trap into the kernel */
    ret