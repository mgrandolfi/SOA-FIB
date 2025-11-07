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

struct task_struct *list_head_to_task_struct(struct list_head *l)
{
    // fem que el list_head apunti al task_struct
    return list_entry(l, struct task_struct, list);
}


struct list_head freequeue; //declarem la llista de processos lliures
struct list_head readyqueue; //declarem la llista de processos a punt per ser executats
extern struct list_head blocked;

struct task_struct *idle_task;
struct task_struct *init_task;

int total_ticks;

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
	struct list_head *first_free_pcb = list_first(&freequeue);
	// l'eliminem ja que no es podrà fer servir per cap altre procés
	list_del(first_free_pcb);
	// fem que el list_head apunti a un a un task struct
	struct task_struct *idle_task_struct = list_head_to_task_struct(first_free_pcb);
	idle_task_struct->PID = 0;
	
	set_quantum(idle_task_struct, DEFAULT_QUANTUM);
  	idle_task_struct->remaining_ticks = 0;
  	idle_task_struct->state = ST_RUN;

	allocate_DIR(idle_task_struct); // li assignem un directori de pàgines
	
	// inicialitzem el context d'execució:
    // col·loquem la direcció de la funció cpu_idle "a sobre" de l'EBP inicial (ret es consumirà després del pop ebp)
	union task_union *idle_task_union = (union task_union*) idle_task_struct; // obtenim la unió task_union del procés idle
    unsigned long *sp = &idle_task_union->stack[KERNEL_STACK_SIZE];
	*--sp = (unsigned long) &cpu_idle;  				// adreça de retorn
	*--sp = 0;                          				// EBP inicial
	idle_task_struct->kernel_esp = (unsigned long)sp;

	idle_task = idle_task_struct;
}

void init_task1(void)
{
	// agafem el primer element lliure de la freequeue
	struct list_head *first_free_pcb = list_first(&freequeue);
	// l'eliminem ja que no es podrà fer servir per cap altre procés
	list_del(first_free_pcb);
	// fem que el list_head apunti a un task struct
	struct task_struct *init_task_struct = list_head_to_task_struct(first_free_pcb);
	init_task_struct->PID = 1;
	
	set_quantum(init_task_struct, DEFAULT_QUANTUM);
  	init_task_struct->remaining_ticks = DEFAULT_QUANTUM;
  	update_process_state_rr(init_task_struct, &readyqueue);

	allocate_DIR(init_task_struct); // li assignem un directori de pàgines

	set_user_pages(init_task_struct); // inicialitzem les pàgines d'usuari

	union task_union *init_task_union = (union task_union*)init_task_struct;
	tss.esp0 = KERNEL_ESP(init_task_union); // actualitzem la pila d'entrada del kernel per a transicions de privilegis

	writeMSR(0x175, tss.esp0); // establim la pila del fast-syscall

	set_cr3(init_task_struct->dir_pages_baseAddr); // convertim l'espai d'adreces al current

	init_task = init_task_struct;
}

void inner_task_switch(union task_union*new)
{
	//fem push ebp al .S
	if (new == (union task_union*)current()) return;
	tss.esp0 = KERNEL_ESP(new);
	writeMSR(0x175, tss.esp0);
	set_cr3(get_DIR(&new->task)); // canviem l'espai d'adreces
	dbg_inner_switched(new);
	switch_context(&current()->kernel_esp, new->task.kernel_esp);
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

void init_stats(struct stats *s) {
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

int get_quantum (struct task_struct t){
    return t->quantum; 
}

void set_quantum (struct task_structt, int new_quantum) {
    t->quantum = new_quantum; 
}

int needs_sched_rr()
{
    struct task_struct c = current();

    // If current exhausted its slice -> schedule
    if (c->remaining_ticks <= 0) return 1;

    // If we're running idle and there is something ready -> schedule
    if (c == idle_task && !list_empty(&readyqueue)) return 1;

    return 0;
}
	
void update_sched_data_rr()
{
    struct task_struct *c = current();
    if (c->remaining_ticks > 0) c->remaining_ticks--;
}

void update_process_state_rr(struct task_struct t, struct list_headdst_queue) {
    // 1) If the task is currently in some queue, unlink it first
    if (t->state != ST_RUN) list_del(&t->list);

    // 2) Decide destination
    if (dst_queue == NULL) {                 // RUNNING: not in any queue
        t->state = ST_RUN;
        return;
    }

    if (dst_queue == &readyqueue) {          // READY: FIFO for RR
        t->state = ST_READY;
        list_add_tail(&t->list, &readyqueue);
    } else if (dst_queue == &blocked) {      // BLOCKED: any wait queue
        t->state = ST_BLOCKED;
        list_add_tail(&t->list, &blocked);
    } else if (dst_queue == &freequeue) {    // FREED PCB
        / state value doesn’t matter operationally while in freequeue */
        t->state = ST_BLOCKED;
        list_add(&t->list, &freequeue);
    } else {
        t->state = ST_BLOCKED;
        list_add_tail(&t->list, dst_queue);
    }
}

void sched_next_rr(void)
{
    struct task_struct next_t;

    if (list_empty(&readyqueue)) {
        // Nothing ready -> run idle
        next_t = idle_task;
    } else {
        // FIFO order for RR
        struct list_heade = list_first(&readyqueue);
        list_del(e);
        next_t = list_head_to_task_struct(e);
    }

    // Prepare next to run
    next_t->state = ST_RUN;
    next_t->remaining_ticks = (next_t->quantum > 0) ? next_t->quantum : DEFAULT_QUANTUM;

    // Context switch
    task_switch((union task_union)next_t);
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


//################################################################################
// Debug functions for task switching
//################################################################################

static int dbg_sw = 0;
void dbg_toggle_switch(void) { dbg_sw ^= 1; }
int  dbg_switch_enabled(void) { return dbg_sw; }

static int uitoa_k(unsigned n, char *b)
{
    if (!n) { b[0]='0'; b[1]=0; return 1; }
    char t[12]; int i=0;
    while (n) { t[i++]='0'+(n%10); n/=10; }
    for (int j=0; j<i; ++j) b[j]=t[i-1-j];
    b[i]=0; return i;
}

void dbg_banner_task_switch(union task_union *new)
{
    if (!dbg_sw) return;
    char a[12], c[12];
    int la=uitoa_k((unsigned)current()->PID, a);
    int lc=uitoa_k((unsigned)new->task.PID, c);
    sys_write_console("[SW] task_switch: from ", 23);
    sys_write_console(a, la);
    sys_write_console(" to ", 4);
    sys_write_console(c, lc);
    sys_write_console("\n", 1);
}

void dbg_inner_switched(union task_union *new)
{
    if (!dbg_sw) return;
    char c[12]; int lc=uitoa_k((unsigned)new->task.PID, c);
    sys_write_console("[SW] inner: TSS/CR3 -> ", 23);
    sys_write_console(c, lc);
    sys_write_console("\n", 1);
}