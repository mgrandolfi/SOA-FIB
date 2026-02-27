/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <types.h>
#include <hardware.h>
#include <segment.h>
#include <sched.h>
#include <mm.h>
#include <io.h>
#include <utils.h>
#include <p_stats.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;

// Free task structs
struct list_head freequeue;
// Ready queue
struct list_head readyqueue;
// Blocked queue for WaitForTick
struct list_head blocked_tick_queue;

extern int dir_pages_used[NR_TASKS];
extern int dir_ref_count[NR_TASKS];

void init_stats(struct stats *s)
{
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

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
	int i;
  // Busquem un directori lliure
  for(i = 0; i < NR_TASKS; i++) {
    if (dir_pages_used[i] == 0) {
      dir_pages_used[i] = 1; // Marquem com ocupat
      dir_ref_count[i] = 1;  // Inicialitzem comptador de referències
      t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[i]; 
      return 1;
    }
  }
  return -1; // No hi ha directoris lliures
}

// Comparteix el directori entre threads del mateix procés
// Incrementa el comptador de referències del DIR
void share_DIR(struct task_struct *t, struct task_struct *source) 
{
  t->dir_pages_baseAddr = source->dir_pages_baseAddr;
  int pos = ((int)t->dir_pages_baseAddr - (int)dir_pages) / (sizeof(page_table_entry) * TOTAL_PAGES);
  if (pos >= 0 && pos < NR_TASKS) {
    dir_ref_count[pos]++;  // Incrementem el comptador de referències
  }
}

void free_DIR(struct task_struct *t)
{
  int pos;
  // Calculem l'índex del directori a partir de la seva adreça
  pos = ((int)t->dir_pages_baseAddr - (int)dir_pages) / (sizeof(page_table_entry) * TOTAL_PAGES);
  
  if (pos >= 0 && pos < NR_TASKS) {
    dir_ref_count[pos]--;  // Decrementem el comptador de referències
    
    // Només alliberem si ningú més usa aquest DIR
    if (dir_ref_count[pos] == 0) {
      dir_pages_used[pos] = 0; // Marquem com lliure
      // Esborrem les entrades d'usuari per evitar que el pròxim procés
      // hereti mapejos "fantasmes" de l'anterior.
      int i;
      page_table_entry *dir = (page_table_entry*)t->dir_pages_baseAddr;
      for (i = NUM_PAG_KERNEL; i < TOTAL_PAGES; i++) {
        dir[i].entry = 0; // Invalidem l'entrada
      }
    }
  }
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

#define DEFAULT_QUANTUM 10

int remaining_quantum=0;

int get_quantum(struct task_struct *t)
{
  return t->total_quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
  t->total_quantum=new_quantum;
}

struct task_struct *idle_task=NULL;

void update_sched_data_rr(void)
{
  remaining_quantum--;
}

int needs_sched_rr(void)
{
  if ((remaining_quantum==0)&&(!list_empty(&readyqueue))) return 1;
  if (remaining_quantum==0) remaining_quantum=get_quantum(current());
  return 0;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue)
{
  if (t->state!=ST_RUN) list_del(&(t->list));
  if (dst_queue!=NULL)
  {
    list_add_tail(&(t->list), dst_queue);
    if (dst_queue!=&readyqueue) t->state=ST_BLOCKED;
    else
    {
      update_stats(&(t->p_stats.system_ticks), &(t->p_stats.elapsed_total_ticks));
      t->state=ST_READY;
    }
  }
  else t->state=ST_RUN;
}

void sched_next_rr(void)
{
  struct list_head *e;
  struct task_struct *t;

  if (!list_empty(&readyqueue)) {
	e = list_first(&readyqueue);
    list_del(e);

    t=list_head_to_task_struct(e);
  }
  else
    t=idle_task;

  t->state=ST_RUN;
  remaining_quantum=get_quantum(t);

  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
  update_stats(&(t->p_stats.ready_ticks), &(t->p_stats.elapsed_total_ticks));
  t->p_stats.total_trans++;

  task_switch((union task_union*)t);
}

void schedule()
{
  update_sched_data_rr();
  if (needs_sched_rr())
  {
    update_process_state_rr(current(), &readyqueue);
    sched_next_rr();
  }
}

void init_idle (void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union*)c;

  c->PID=0;
  c->TID=0; /* Idle task has TID 0 */

  c->total_quantum=DEFAULT_QUANTUM;

  init_stats(&c->p_stats);

  allocate_DIR(c);
  
  // Inicialitzem els camps de threads per la tasca idle
  c->process_leader = c;  // l'idle és el seu propi líder
  INIT_LIST_HEAD(&c->thread_list);
  c->num_threads = 1;
  c->user_stack_base = 0;  // idle no usa pila d'usuari
  c->user_stack_pages = 0;
  c->user_stack_max_pages = 0;

  uc->stack[KERNEL_STACK_SIZE-1]=(unsigned long)&cpu_idle; /* Return address */

  uc->stack[KERNEL_STACK_SIZE-2]=0; /* register ebp */

  c->register_esp=(int)&(uc->stack[KERNEL_STACK_SIZE-2]); /* top of the stack */

  idle_task=c;
}

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void init_task1(void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union*)c;

  c->PID=1;
  c->TID=1; // TID == PID pel main thread

  c->total_quantum=DEFAULT_QUANTUM;

  c->state=ST_RUN;

  remaining_quantum=c->total_quantum;

  init_stats(&c->p_stats);

  allocate_DIR(c);

  set_user_pages(c);
  
  c->process_leader = c;  // el main thread és el líder del seu procés
  INIT_LIST_HEAD(&c->thread_list);
  c->num_threads = 1;  // comencem amb un sol thread
  c->stack_id = 0;     // El main thread sempre té stack_id 0
  c->threads_mask = 1; // Marquem el bit 0 com ocupat (1 << 0)
  
  allocate_thread_stack(c, 0);

  tss.esp0=(DWord)&(uc->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(uc->stack[KERNEL_STACK_SIZE]));

  set_cr3(c->dir_pages_baseAddr);
}

void init_freequeue()
{
  int i;

  INIT_LIST_HEAD(&freequeue);

  /* Insert all task structs in the freequeue */
  for (i=0; i<NR_TASKS; i++)
  {
    task[i].task.PID=-1;
    list_add_tail(&(task[i].task.list), &freequeue);
  }
}

void init_sched()
{
  init_freequeue();
  INIT_LIST_HEAD(&readyqueue);
  INIT_LIST_HEAD(&blocked_tick_queue);
}

struct task_struct* current()
{
  int ret_value;
  
  return (struct task_struct*)( ((unsigned int)&ret_value) & 0xfffff000);
}

struct task_struct* list_head_to_task_struct(struct list_head *l)
{
  return (struct task_struct*)((int)l&0xfffff000);
}

/* Do the magic of a task switch */
void inner_task_switch(union task_union *new)
{
  page_table_entry *new_DIR = get_DIR(&new->task);
  page_table_entry *current_DIR = get_DIR(current());

  /* Update TSS and MSR to make it point to the new stack */
  tss.esp0=(int)&(new->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(new->stack[KERNEL_STACK_SIZE]));

  /* TLB flush. New address space */
  // OPTIMITZACIÓ: Només fem flush si canviem de procés (diferent directori)
  if (new_DIR != current_DIR) {
    set_cr3(new_DIR);
  }

  switch_stack(&current()->register_esp, new->task.register_esp);
}


/* Force a task switch assuming that the scheduler does not work with priorities */
void force_task_switch()
{
  update_process_state_rr(current(), &readyqueue);

  sched_next_rr();
}

/******************************************************************************/
/* Funcions per a la gestió de les piles dels THREADS                         */
/******************************************************************************/

// Assigna la pila d'usuari per un thread
// stack_num: número de pila (0 per main thread, 1+ per threads addicionals)
// Els threads addicionals (stack_num >= 1) necessiten la seva pròpia pila:
// - No poden compartir la pila del main thread (col·lisions de variables locals -> race conditions)
// - Cada thread té la seva pròpia regió amb gap per creixement dinàmic
int allocate_thread_stack(struct task_struct *thread, int stack_num)
{
  page_table_entry *process_PT = get_PT(thread);
  int new_ph_pag;
  unsigned int stack_base_page;
  
  // Per threads (inclòs el main thread), calculem la posició de la pila
  // Cada thread necessita: THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES
  // Layout: [pàgines de gap] [pàgines de pila]
  // La pila creix cap avall, així que assignem des del top
  unsigned int thread_region_pages = THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES;

  // Càlcul de stack_base_page:
  // - FIRST_THREAD_STACK_PAGE: pàgina inicial de la regió de threads
  // - + stack_num * thread_region_pages: salta les regions dels threads anteriors
  // - + thread_region_pages: ens situa al TOP de la regió actual (pila creix cap avall)
  // Exemple: stack_num = 0 -> pàg 20 + 0*11 + 11 = pàg 31 (top de la 1a regió)
  stack_base_page = FIRST_THREAD_STACK_PAGE + stack_num * thread_region_pages + thread_region_pages;
  
  // Assignem la pàgina inicial (THREAD_STACK_INITIAL_PAGES = 1)
  // La pila creix cap avall des de stack_base_page
  new_ph_pag = alloc_frame();
  if (new_ph_pag == -1) {
    return -1;  // No hi ha memòria disponible
  }
  // Mapegem la pàgina: pila creix cap avall, primera pàgina a (stack_base_page - 1)
  set_ss_pag(process_PT, stack_base_page - 1, new_ph_pag);
  
  // Guardem la info de la pila del thread
  thread->user_stack_base = stack_base_page << 12;  // convertim pàgina a adreça
  thread->user_stack_pages = THREAD_STACK_INITIAL_PAGES;
  thread->user_stack_max_pages = THREAD_STACK_MAX_PAGES;
  
  return 0;
}

// Allibera les pàgines de la pila d'usuari d'un thread
void free_thread_stack(struct task_struct *thread)
{
  page_table_entry *process_PT = get_PT(thread);
  unsigned int stack_base_page = thread->user_stack_base >> 12;
  int i;
  
  // Alliberem totes les pàgines assignades
  for (i = 0; i < thread->user_stack_pages; i++) {
    unsigned int page = stack_base_page - 1 - i;
    free_frame(get_frame(process_PT, page));
    del_ss_pag(process_PT, page);
  }
  
  thread->user_stack_pages = 0;
}

// is_stack_growth_fault - Comprova si un page fault és degut a creixement de pila
// Cada thread té una regió amb estructura [GAP][PILA_ACTUAL]
// - La pila comença amb 1 pàgina i pot créixer cap avall dins del gap
// - Si hi ha un page fault al gap, és legítim i hem de créixer la pila
// - Si el fault és fora del gap, és un error de memòria real
int is_stack_growth_fault(unsigned int faulting_address, struct task_struct *task)
{
  unsigned int fault_page = faulting_address >> 12;
  unsigned int stack_base_page = task->user_stack_base >> 12;
  unsigned int current_bottom_page = stack_base_page - task->user_stack_pages;
  unsigned int gap_bottom_page = stack_base_page - task->user_stack_max_pages;
  
  // Comprovem si el fault és a la regió de gap abans de la pila actual
  if (fault_page >= gap_bottom_page && fault_page < current_bottom_page) {
    return 1;
  }
  
  return 0;
}

// Fa créixer la pila d'usuari una pàgina
// Assigna una pàgina física nova i la mapeja per fer créixer la pila
// Retorna: 0 si tot va bé, -1 si falla
int grow_user_stack(struct task_struct *task, unsigned int faulting_address)
{
  page_table_entry *process_PT = get_PT(task);
  unsigned int fault_page = faulting_address >> 12;
  int new_ph_pag;
  
  // DEBUG: Descomentar per veure info del creixement
  /*
  printk("GROW_STACK: TID=");
  printk_int(task->TID);
  printk(" pages=");
  printk_int(task->user_stack_pages);
  printk("/");
  printk_int(task->user_stack_max_pages);
  printk(" fault_pg=0x");
  printk_hex(fault_page);
  */
  
  // Comprovem si encara podem créixer
  if (task->user_stack_pages >= task->user_stack_max_pages) {
    // printk(" -> LIMIT EXCEEDED!\n");
    return -1;  // ja no podem créixer més
  }
  
  // Assignem una pàgina física nova
  new_ph_pag = alloc_frame();
  if (new_ph_pag == -1) {
    // printk(" -> NO MEMORY!\n");
    return -1;  // no hi ha memòria disponible
  }
  
  // Mapegem la pàgina nova a l'adreça del fault
  set_ss_pag(process_PT, fault_page, new_ph_pag);
  
  // Actualitzem la info de la pila
  task->user_stack_pages++;
  
  // printk(" -> OK (new_pages=");
  // printk_int(task->user_stack_pages);
  // printk(")\n");
  
  // Fem flush de la TLB per la pàgina nova
  set_cr3(get_DIR(task));
  
  return 0;
}

