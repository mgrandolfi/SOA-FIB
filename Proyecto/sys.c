/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#include <segment.h>

#define LECTURA 0
#define ESCRIPTURA 1

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1 && fd!=10) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return current()->PID;
}

int global_PID=1000;

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  struct task_struct *current_task = current();
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current_task, uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      free_DIR((struct task_struct*)uchild);
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  // Comparteix pàgines de kernel i codi (només mapeig, no copia)
  page_table_entry *parent_PT = get_PT(current_task);
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  
  // Copia dades: mapeja temporalment pàgines del fill a l'espai del pare
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  set_cr3(get_DIR(current_task));

  // Assigna nou PID i inicialitza com a procés amb un sol thread
  if (global_PID == 2147483647) { // MAX_INT
    for (i=0; i<NUM_PAG_DATA; i++) {
      free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
      del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
    }
    free_DIR((struct task_struct*)uchild);
    list_add_tail(lhcurrent, &freequeue);
    return -EAGAIN;
  }
  uchild->task.PID=++global_PID;
  uchild->task.TID=uchild->task.PID;
  uchild->task.state=ST_READY;
  
  uchild->task.process_leader = &uchild->task;
  INIT_LIST_HEAD(&uchild->task.thread_list);
  uchild->task.num_threads = 1;
  
  // Heretem el stack_id del pare per mantenir la coherència d'adreces
  // Això és necessari perquè les adreces de la pila (que depenen de stack_id)
  // siguin vàlides en el nou espai d'adreces.
  uchild->task.stack_id = current_task->stack_id;
  uchild->task.threads_mask = (1 << current_task->stack_id); // Marquem només aquest slot com ocupat
  
  // Resetejem l'estat del teclat per al fill
  uchild->task.keyboard_callback = NULL;
  uchild->task.keyboard_wrapper = NULL;
  uchild->task.keyboard_aux_stack = 0;
  uchild->task.in_keyboard_callback = 0;
  
  // Configura pila d'usuari usant el mateix stack_id que el pare
  if (allocate_thread_stack(&uchild->task, uchild->task.stack_id) < 0) {
    for (i=0; i<NUM_PAG_DATA; i++) {
      free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
      del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
    }
    free_DIR((struct task_struct*)uchild);
    list_add_tail(lhcurrent, &freequeue);
    return -ENOMEM;
  }
  
  // Ara hem de copiar el contingut de la pila del pare a la del fill.
  // Com que tenen el mateix stack_id, les adreces virtuals coincideixen.
  
  // Obtenim la base de la pila del pare
  unsigned int stack_base_page = current_task->user_stack_base >> 12;
  int stack_pages = current_task->user_stack_pages;
  
  // Si el pare té més d'una pàgina de pila, hem d'assignar les pàgines extra al fill
  // allocate_thread_stack només n'ha assignat 1.
  for (i = 1; i < stack_pages; i++) {
    int new_ph_pag = alloc_frame();
    if (new_ph_pag == -1) {
      int j;
      for (j = 1; j < i; j++) {
        free_frame(get_frame(process_PT, stack_base_page - 1 - j));
        del_ss_pag(process_PT, stack_base_page - 1 - j);
      }
      free_frame(get_frame(process_PT, stack_base_page - 1));
      del_ss_pag(process_PT, stack_base_page - 1);
      for (j=0; j<NUM_PAG_DATA; j++) {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+j));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+j);
      }
      free_DIR((struct task_struct*)uchild);
      list_add_tail(lhcurrent, &freequeue);
      return -ENOMEM;
    }
    set_ss_pag(process_PT, stack_base_page - 1 - i, new_ph_pag);
  }
  // Actualitzem el comptador de pàgines del fill
  uchild->task.user_stack_pages = stack_pages;
  
  // Copiem les pàgines de la pila
  for (i = 0; i < stack_pages; i++) {
    unsigned int page = stack_base_page - 1 - i;
    
    int temp_page = PAG_LOG_INIT_DATA + NUM_PAG_DATA;
    
    set_ss_pag(parent_PT, temp_page, get_frame(process_PT, page)); 
    
    // Copiem: origen (pila pare) -> destí (finestra temporal que apunta a pila fill)
    copy_data((void*)(page<<12), (void*)(temp_page<<12), PAGE_SIZE);
    
    del_ss_pag(parent_PT, temp_page);
  }
  set_cr3(get_DIR(current_task)); // Flush TLB

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current_task) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  
  return uchild->task.PID;
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
char localbuffer [TAM_BUFFER];
int bytes_left;
int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	if (fd == 10) {
    // Escriptura directa a memòria de video VGA (0xb8000)
    // Copiem tot el buffer de pantalla en una única operació
		int screen_size = 80 * 25 * 2;
		if (nbytes > screen_size) nbytes = screen_size;
		copy_from_user(buffer, (void*)0xb8000, nbytes);
		return nbytes;
	}

	bytes_left = nbytes;
	while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	}
	if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	}
	return (nbytes-bytes_left);
}


extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

void sys_exit()
{  
  int i;
  struct task_struct *current_task = current();
  page_table_entry *process_PT = get_PT(current_task);

  // SI SOM EL LEADER DEL PROCÉS, MATAR TOTS ELS THREADS SECUNDARIS
  if (current_task->process_leader == current_task) {
    struct list_head *pos, *tmp;
    
    // Iterem per tots els threads del procés (menys el leader)
    list_for_each_safe(pos, tmp, &current_task->thread_list) {
      struct task_struct *thread = list_entry(pos, struct task_struct, thread_list);
      
      // Alliberar la pila del thread secundari
      if (thread->TID != thread->PID) {  // No és el main thread
        free_thread_stack(thread);
      }

      // Decrementem el comptador de referències del directori per cada thread
      free_DIR(thread);
      
      // Treure el thread de la cua on estigui
      if (!list_empty(&thread->list)) {
        list_del(&thread->list);
      }
      
      // Treure de la llista de threads del procés
      list_del(&thread->thread_list);
      
      // Marcar com a zombie i retornar a freequeue
      thread->PID = -1;
      list_add_tail(&thread->list, &freequeue);
    }
  }
  
  // Alliberar la pila del thread actual (si no és el main amb stack_id 0)
  free_thread_stack(current_task);
  
  // Alliberar la pila auxiliar de teclat si existeix (només n'hi ha una per procés)
  if (current_task->keyboard_aux_stack != 0) {
    unsigned int page = current_task->keyboard_aux_stack >> 12;
    int frame = get_frame(process_PT, page - 1);
    if (frame != -1) {
        free_frame(frame);
        del_ss_pag(process_PT, page - 1);
        set_cr3(get_DIR(current_task)); // Flush TLB
    }
    current_task->keyboard_aux_stack = 0;
  }

  // Alliberar pàgines de dades del procés
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  // Alliberem el directori (decrementem comptador de referències)
  free_DIR(current_task);
  
  // Retornar el task_struct del leader a la freequeue
  list_add_tail(&(current_task->list), &freequeue);
  
  current_task->PID=-1;

  sched_next_rr();
}


int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; 
}

int global_TID = 1000;

int ret_from_thread(void)
{
  return 0;
}

int sys_thread_create(void (*function)(void *), void *parameter, void *wrapper)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  struct task_struct *current_task = current();
  struct task_struct *process_leader = current_task->process_leader;
  

  if (!access_ok(VERIFY_READ, function, sizeof(void*))) return -EFAULT;
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent = list_first(&freequeue);
  list_del(lhcurrent);
  uchild = (union task_union*)list_head_to_task_struct(lhcurrent);

  // Copia la pila de kernel (context inicial)
  copy_data(current_task, uchild, sizeof(union task_union));
  
  // Comparteix directori de pàgines (threads del mateix procés)
  // IMPORTANT: Usem share_DIR per incrementar el comptador de referències
  share_DIR(&uchild->task, current_task);
  
  if (global_TID == 2147483647) { // MAX_INT
    list_add_tail(lhcurrent, &freequeue);
    return -EAGAIN;
  }

  // Assigna identificadors: TID únic, mateix PID que el procés
  uchild->task.TID = ++global_TID;
  uchild->task.PID = process_leader->PID;
  uchild->task.process_leader = process_leader;
  
  INIT_LIST_HEAD(&uchild->task.thread_list);
  uchild->task.num_threads = 0;  // Threads secundaris no gestionen aquesta llista
  
  // Resetejem l'estat del teclat per al nou thread, però ens quedem amb la pila auxiliar compartida
  uchild->task.keyboard_callback = NULL;
  uchild->task.keyboard_wrapper = NULL;
  uchild->task.in_keyboard_callback = 0;
  // Nota: keyboard_aux_stack s'hereta (shared mapping)
  
  // Busquem un stack_id lliure
  int stack_id = -1;
  int i;
  for (i = 0; i < 32; i++) { // Assumim màxim 32 threads per procés (per la mida de l'int)
    if (!((process_leader->threads_mask >> i) & 1)) {
      stack_id = i;
      break;
    }
  }
  
  if (stack_id == -1) { // No hi ha slots lliures
    free_DIR((struct task_struct*)uchild); // Desfer share_DIR
    list_add_tail(lhcurrent, &freequeue);
    return -ENOMEM;
  }
  
  // Assigna pila d'usuari pròpia per al thread
  if (allocate_thread_stack(&uchild->task, stack_id) < 0) {
    free_DIR((struct task_struct*)uchild); // Desfer share_DIR
    list_add_tail(lhcurrent, &freequeue);
    return -ENOMEM;
  }
  
  // Marquem l'slot com ocupat
  process_leader->threads_mask |= (1 << stack_id);
  uchild->task.stack_id = stack_id;
  
  process_leader->num_threads++;
  list_add_tail(&uchild->task.thread_list, &process_leader->thread_list);
  
  // Construeix fake interrupt frame a la pila de kernel
  // Aquest frame serà consumit per thread_start_trampoline
  unsigned int new_user_esp = uchild->task.user_stack_base - 16;
  
  // Reserva espai a la pila d'usuari per paràmetre i funció (per al wrapper)
  new_user_esp -= sizeof(DWord);
  *(DWord*)new_user_esp = (DWord)parameter;
  
  new_user_esp -= sizeof(DWord);
  *(DWord*)new_user_esp = (DWord)function;
  
  new_user_esp -= sizeof(DWord);
  *(DWord*)new_user_esp = 0; // Fake return address per al wrapper
  
  uchild->task.register_esp = (int)&uchild->stack[KERNEL_STACK_SIZE];
  
  // Context hardware
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_DS;  /* SS */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = new_user_esp; /* ESP */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0x200;  /* EFLAGS (IF=1, interrupts enabled) */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_CS;  /* CS */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = (DWord)wrapper;  /* EIP -> Wrapper */
  
  // Context software
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_DS;  /* GS */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_DS;  /* FS */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_DS;  /* ES */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = __USER_DS;  /* DS */
  
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* EAX */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* EBP */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* EDI */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* ESI */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* EDX */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* ECX */
  
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;  /* EBX */
  
  // Adreça de retorn per switch_stack: salta al trampoline
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = (DWord)&thread_start_trampoline;
  
  // Guardem ebp 
  uchild->task.register_esp -= sizeof(DWord);
  *(DWord*)(uchild->task.register_esp) = 0;
  
  init_stats(&(uchild->task.p_stats));

  uchild->task.state = ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  
  return uchild->task.TID;
}

void sys_thread_exit(void)
{
  struct task_struct *current_task = current();
  struct task_struct *process_leader = current_task->process_leader;
  
  // Si és l'últim thread del procés, acabar tot el procés
  if (process_leader->num_threads == 1) {
    sys_exit();
    // sys_exit no retorna, però per seguretat:
    //return;
  }

  // Decrementem el comptador de referències del directori
  free_DIR(current_task);

  free_thread_stack(current_task);
  
  // NO alliberem la pila auxiliar aqui perque es compartida entre threads del mateix proces.
  // S'alliberara a sys_exit quan mori tot el proces.

  // Alliberem l'slot de la màscara del líder
  process_leader->threads_mask &= ~(1 << current_task->stack_id);
  
  // Eliminar el thread de la llista del procés
  if (!list_empty(&current_task->thread_list)) {
    list_del(&current_task->thread_list);
  }
  
  process_leader->num_threads--;
  
  // Retornar el task_struct a la freequeue i invalidar identificadors
  list_add_tail(&(current_task->list), &freequeue);
  current_task->PID = -1;
  current_task->TID = -1;
  
  // Canviar al següent thread/procés (no retorna aquí mai)
  sched_next_rr();
}

int sys_get_stack_id(void)
{
  return current()->stack_id;
}

#define KEYBOARD_AUX_STACK_PAGE 700

// Variable externa per saber si hi ha un callback actiu
extern int keyboard_callback_active;

int sys_keyboard_event(void (*wrapper)(void), void (*func)(char key, int pressed)) {
  struct task_struct *t = current();
  
  // Si ja hi ha un callback actiu al sistema, retornem error
  // (ha de ser la primera comprovació)
  if (keyboard_callback_active) return -EINPROGRESS;
  
  // Si func es NULL, desactivem el callback
  if (func == NULL) {
    t->keyboard_callback = NULL;
    t->keyboard_wrapper = NULL;
    return 0;
  }
  
  if (!access_ok(VERIFY_READ, func, 1)) return -EFAULT;
  
  t->keyboard_callback = func;
  t->keyboard_wrapper = wrapper;
  
  // Si no tenim pila auxiliar, la creem
  if (t->keyboard_aux_stack == 0) {
    int new_ph_pag = alloc_frame();
    if (new_ph_pag == -1) return -ENOMEM;
    
    page_table_entry *process_PT = get_PT(t);
    set_ss_pag(process_PT, KEYBOARD_AUX_STACK_PAGE, new_ph_pag);
    
    // Flush TLB per assegurar que la nova pàgina és visible
    set_cr3(get_DIR(t));
    
    t->keyboard_aux_stack = (KEYBOARD_AUX_STACK_PAGE + 1) << 12;
  }
  return 0;
}

int is_in_keyboard_callback() {
  return current()->in_keyboard_callback;
}

int sys_WaitForTick()
{
  struct task_struct *t = current();
  update_process_state_rr(t, &blocked_tick_queue);
  sched_next_rr();
  return 0;
}