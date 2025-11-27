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

#define LECTURA 0
#define ESCRIPTURA 1

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; 
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
 //coment everyting out           
int sys_ThreadCreate(void (*function)(void* arg), void *parameter)
{
  //s hauria de comprovar errors en els parametres de entrada 


  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  struct list_head *lhcurrent = list_first(&freequeue);
  list_del(lhcurrent);
  
  //newthread = nt
  struct task_struct *new_t = list_head_to_task_struct(lhcurrent);
  union task_union *uthread = (union task_union*)new_t;
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uthread, sizeof(union task_union));
  
  // 3. COMPARTIR l'espai d'adreces (Clau per als threads)new_t->dir_pages_baseAddr = current()->dir_pages_baseAddr;
 // 4. Calcular la posició de la pila d'usuari
    // Utilitzem l'index dins l'array 'task' per garantir que no es solapin.
    int task_idx = (int)(new_t - &task[0]); 
    
    // Formula: Base - (Index * (Max_Size + Gap))
    unsigned long stack_top = THREAD_STACK_BASE - (task_idx * (THREAD_STACK_MAX + THREAD_STACK_GAP));
    
    // 5. Assignar UNA pàgina física inicial per a la pila
    // Només assignem la pàgina més alta (stack_top). El creixement es farà per Page Fault.
    int page_logical = (stack_top - PAGE_SIZE) >> 12;
    page_table_entry *pt = get_PT(new_t);

    // Mirem si ja tenim un frame assignat (per si es reutilitza la task_struct sense netejar)
    if (get_frame(pt, page_logical) == 0) {
        int new_frame = alloc_frame();
        if (new_frame == -1) {
            list_add_tail(lh, &freequeue);
            return -ENOMEM;
        }
        set_ss_pag(pt, page_logical, new_frame);
    }

    // 6. Preparar la pila d'usuari (Simular el 'push' dels paràmetres)
    // Com que compartim espai d'adreces, podem escriure directament a l'adreça lògica
    // PERÒ, hem d'anar amb compte amb la TLB.
    
    // Mapegem temporalment a una pàgina segura per escriure sense Page Faults estranys
    // o escrivim directament si estem segurs que la pàgina és vàlida.
    // Farem el mètode segur de mapatge temporal:
    
    int temp_page = (THREAD_STACK_BASE >> 12) + 1; // Una pàgina lliure temporal
    set_ss_pag(pt, temp_page, get_frame(pt, page_logical));
    
    // Necessitem refrescar la TLB per veure aquest mapatge temporal
    set_cr3(get_DIR(current()));


    //caldria construir la pila d'usuari amb els paràmetres
    user_stack--;
    *(unsigned long*)user_stack = (unsigned long)parameter; // paràmetre
    user_stack--;
    *(unsigned long*)user_stack = 0; // Retorn fictici


    // calcular el nou valor de ESP
    new_t->register_esp = (unsigned long)user_stack;

    // Desfem el mapatge temporal
    del_ss_pag(pt, temp_page);
    set_cr3(get_DIR(current())); // Flush TLB

    new_t->PID = ++global_PID;
    new_t->state = ST_READY;

    // Preparem la pila de kernel per al 'iret' (Hardware Context)
    // Posició: Base de la pila de kernel + Tamany de la pila de kernel - Context Hardware

}




int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
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
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

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

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  /* Free task_struct */
  list_add_tail(&(current()->list), &freequeue);
  
  current()->PID=-1;
  
  /* Restarts execution of the next process */
  sched_next_rr();
}

/* System call to force a task switch */
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
  return -ESRCH; /*ESRCH */
}
