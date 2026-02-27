#include <asm.h>
#include <segment.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $SYSENTER_RETURN
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(SYSENTER_RETURN)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

/* int write(int fd, char *buffer, int size) */
ENTRY(write)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $4, %eax
	movl 0x8(%ebp), %ebx;	//fd
	movl 0xC(%ebp), %ecx;	//buffer
	movl 0x10(%ebp), %edx;	//size
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

/* Common code for negative return */
nok:
	neg %eax
	push %eax
	call __errno_location
	pop %edx
	mov %edx, (%eax)
	mov $-1, %eax
	popl %ebp
	ret

/* int gettime() */
ENTRY(gettime)
	pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int getpid() */
ENTRY(getpid)
	pushl %ebp
	movl %esp, %ebp
	movl $20, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	movl $2, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* void exit() */
ENTRY(exit)
	pushl %ebp
	movl %esp, %ebp
	movl $1, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int yield() */
ENTRY(yield)
	pushl %ebp
	movl %esp, %ebp
	movl $13, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int get_stats(int pid, struct stats *st) */
ENTRY(get_stats)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $35, %eax
	movl 0x8(%ebp), %ebx;	//pid
	movl 0xC(%ebp), %ecx;	//st
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* int ThreadCreate(void (function)(void *), void *parameter) */
ENTRY(ThreadCreate)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Guardem EBX
	movl $36, %eax
	movl 0x8(%ebp), %ebx;	// function pointer
	movl 0xC(%ebp), %ecx;	// parameter
	leal thread_wrapper, %edx; // Li passem l'adreça del wrapper
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) --> error
	popl %ebp
	ret

ENTRY(thread_wrapper)
	pushl %ebp
	movl %esp, %ebp
	
	// Inicialitzem errno a 0 per al nou thread
	call __errno_location
	movl $0, (%eax)

	// Estructura de la pila:
	// 12(%ebp) -> paràmetre
	// 8(%ebp)  -> funció a executar
	// 4(%ebp)  -> adreça de retorn (falsa)
	// 0(%ebp)  -> ebp antic
	
	pushl 12(%ebp)    // Passem el paràmetre a la pila
	call *8(%ebp)     // Cridem a la funció de l'usuari (el void (*function)(void *))
	addl $4, %esp     // Netegem el paràmetre de la pila
	
	call ThreadExit   // Si la funció retorna, forcem la sortida del thread
	
	popl %ebp
	ret

/* void ThreadExit(void) */
ENTRY(ThreadExit)
	pushl %ebp
	movl %esp, %ebp
	movl $37, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	// No hauria de retornar, però per si de cas...:
	popl %ebp
	ret

ENTRY(get_stack_id)
	pushl %ebp
	movl %esp, %ebp
	movl $38, %eax
	call syscall_sysenter
	// No comprovem errno. Evitem recursivitat infinita
	popl %ebp
	ret

ENTRY(keyboard_wrapper_asm)
	call *%eax
	int $0x2b
	// No hauria de retornar

/* int KeyboardEvent(void (*func)(char key, int pressed)) */
ENTRY(KeyboardEvent)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl $39, %eax
	movl $keyboard_wrapper_asm, %ebx // Passem el wrapper
	movl 0x8(%ebp), %ecx             // Passem la funcio d'usuari
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int WaitForTick() */
ENTRY(WaitForTick)
	pushl %ebp
	movl %esp, %ebp
	movl $40, %eax
	call syscall_sysenter
	popl %ebp
	ret