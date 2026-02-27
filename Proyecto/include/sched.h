/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>
#include <interrupt.h>


#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list;	/* Task struct enqueuing */
  int register_esp;		/* position in the stack */
  enum state_t state;		/* State of the process */
  int total_quantum;		/* Total quantum of the process */
  struct stats p_stats;		/* Process stats */
  
  // Camps per suport de threads - cada thread té el seu propi TID
  int TID;			// Thread ID únic per identificar cada thread
  struct task_struct *process_leader; // Pointer al thread principal del procés
  struct list_head thread_list;	// Llista de threads que pertanyen a aquest procés
  int num_threads;		// Número de threads del procés (només vàlid pel líder)
  
  // Gestió de piles de threads
  int stack_id;           // ID de la pila assignada a aquest thread (0, 1, 2...)
  unsigned long threads_mask; // Màscara de bits per saber quins stack_id estan ocupats (només líder)

  // Gestió de la pila d'usuari per tenir creixement dinàmic
  unsigned int user_stack_base;	// Adreça base de la pila (adreça més alta)
  unsigned int user_stack_pages;	// Nombre de pàgines actualment assignades
  unsigned int user_stack_max_pages; // Màxim de pàgines permeses per creixemet

  // Suport per teclat
  void (*keyboard_callback)(char, int); // Funcio de callback d'usuari
  void (*keyboard_wrapper)(void);       // Wrapper en assemblador (rep callback a %eax)
  unsigned int keyboard_aux_stack;      // Pila auxiliar per al callback
  int in_keyboard_callback;             // Flag d'execucio del callback
  struct pt_regs saved_regs;            // Registres guardats durant el callback
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct list_head blocked_tick_queue;

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void schedule(void);

struct task_struct * current();

void task_switch(union task_union*t);
void switch_stack(int * save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);
void share_DIR(struct task_struct *t, struct task_struct *source);
void free_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

// Funcions per a la gestió de les piles dels THREADS
int allocate_thread_stack(struct task_struct *thread, int stack_num);
void free_thread_stack(struct task_struct *thread);
int is_stack_growth_fault(unsigned int faulting_address, struct task_struct *task);
int grow_user_stack(struct task_struct *task, unsigned int faulting_address);
void thread_start_trampoline(void);

#endif  /* __SCHED_H__ */
