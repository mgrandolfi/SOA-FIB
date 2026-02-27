#include <asm.h>
#include <segment.h>

/* void switch_stack(void ** save_sp, void * new_sp) NOT REALLY A FUNCTION!!! */
ENTRY(switch_stack)
	mov 4(%esp), %eax
	mov %ebp, (%eax)
	mov 8(%esp), %esp
	popl %ebp
	ret

/* void task_switch(union task_union *new) */
ENTRY(task_switch)
	pushl %ebp
	movl %esp, %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	pushl 8(%ebp)
	call inner_task_switch
	addl $4, %esp
	popl %ebx
	popl %edi
	popl %esi
	popl %ebp
	ret

/* void * get_ebp(void) */
ENTRY(get_ebp)
	mov %ebp, %eax
	ret

/* void setMSR(unsigned long msr_number, unsigned long high, unsigned long low)*/
ENTRY(setMSR)
	push %ebp
	movl %esp, %ebp
	movl 0x10(%ebp), %eax
	movl 0xc(%ebp), %edx
	movl 0x8(%esp), %ecx
	wrmsr
	pop %ebp
	ret

// thread_start_trampoline - Punt d'entrada per a threads nous 
// Executem aquesta funció quan un thread
// nou arriba a la CPU per primera vegada després de task_switch()
//	 - El "ret" de switch_stack salta aquí perquè vam col·locar @thread_start_trampoline a la pila
//   - Restaurem tots els registres
//   - IRET salta a mode usuari amb la pila d'usuari i comença a executar la funció del thread
// Fem servir IRET perquè és l'única instrucció que pot fer el salt de kernel a usuari
// simulant un return d'una interrupció
ENTRY(thread_start_trampoline)
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
	popl %ds
	popl %es
	popl %fs
	popl %gs
	iret         // Salta a la funció del thread en mode usuari

// unsigned int get_cr2(void) - Retorna el valor del registre CR2
ENTRY(get_cr2)
	movl %cr2, %eax
	ret
