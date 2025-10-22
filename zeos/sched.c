/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

struct list_head freequeue; //declarem la llista de processos lliures
struct list_head readyqueue; //declarem la llista de processos a punt per ser executats
extern struct list_head blocked;

struct task_struct *idle_task;

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
	// agafem el primer element lliure de la freequeue
	struct list_head *free_list = list_first(&freequeue);
	// l'eliminem ja que no es podrà fer servir per cap altre procés
	list_del(free_list);
	// fem que el list_head apunti a un a un task struct
	struct task_struct *idle_task_struct = list_head_to_task_struct(free_list);
	idle_struct.PID = 0;
	
	allocate_DIR(idle_task_struct); // li assignem un directori de pàgines
	
	// inicialitzem el context d'execució:
    // col·loquem la direcció de la funció cpu_idle al tope de la pila del procés
	union task_union *idle_task_union = (union task_union*) idle_task_struct; // obtenim la unió task_union del procés idle
    idle_task_union->stack[KERNEL_STACK_SIZE - 1] = (unsigned long)&cpu_idle;
}

void init_task1(void)
{
	// (1) Assign PID = 1 to the init process.
	task[1].task.PID = 1;
	// (2) Allocate a new page directory for the process address space.
	allocate_DIR(&task[1].task);
	// (3) Set up its user address space (code and data pages).
	set_user_pages(&task[1].task);
	// (4) Update the TSS to point to the new kernel stack and configure sysenter MSR.
	tss.esp0 = KERNEL_ESP(&task[1]);
	writeMSR(0x175, INITIAL_ESP);
	// (5) Set its page directory as the current directory.
	set_cr3(task[1].task.dir_pages_baseAddr);
}

void task_switch(union task_union*t)
{
	//fem push ebp al .S
	set_cr3(t->task.dir_pages_baseAddr);
	tss.esp0 = KERNEL_ESP(&t->task);
	writeMSR(0x175, KERNEL_ESP(&t->task));
	switch_stack(&(current()->stack_pointer),t->stack);
}

void init_sched()
{
	INIT_LIST_HEAD(&freequeue);  //inicialitzem la llista de processos lliures
	INIT_LIST_HEAD(&readyqueue); //inicialitzem la llista de processos a punt per ser executats
	INIT_LIST_HEAD(&blocked);    //inicialitzem la llista de processos bloquejats
	
	for (int i = 0; i < NR_TASKS; ++i) {
		// task[i].task.list:
		// 1. union task_union task[i] (NR_TASK especific)
		// 2. task = task_struct 
		// 3. list_head que apunta al task_struct
		list_add(&(task[i].task.list), &freequeue);
	}
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

