#include <asm.h>

/*ENTRY(save_regs)
	pushl %ebp
	movl %esp, %ebp
	
	pushl %ebx
	pushl %esi
	pushl %edi
	
	popl %edi
	popl %esi
	popl %ebx
	
	movl %ebp, %esp
	popl %ebp
	ret*/

ENTRY(save_regs)
	//obtenemos la direccion de retorno,
    //esta en (%esp) porque call la acaba de pushear
	movl (%esp), %eax   //cargamos la dirección de retorno en EAX
	addl $4, %esp       //saca la direccion de retorno de la pila

	//guardamos los registros pedidos
	pushl %ebx
	pushl %esi
	pushl %edi          //ESP queda apuntando a EDI

	//volvemos a task_switch saltando, no retornando
	jmp *%eax

/*ENTRY(restore_regs)
	pushl %ebp
	movl %esp, %ebp
	
	movl %ebp, %esp
	popl %ebp
	ret*/

ENTRY(restore_regs)
	popl %edi
	popl %esi
	popl %ebx
	
	//ahora la pila solo contiene [direccion_retorno]
	//hacemos ret para volver a task_switch
	ret