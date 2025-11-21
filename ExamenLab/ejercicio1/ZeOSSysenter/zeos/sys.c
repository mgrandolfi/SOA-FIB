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

extern struct list_head blocked; //cola global de bloqueados

struct task_struct* find_task_by_pid(int pid) {
    for (int i = 0; i < NR_TASKS; i++) {
      if (task[i].task.PID == pid) {
        return &task[i].task;
      }
    }
  //return NULL; no hay control de errores. supongo que el pid que pasamos siempre será válido
}

int sys_writeTo(int pid, char *src, int size) {
  //buscar el PCB del lector
  struct task_struct *reader_task = find_task_by_pid(pid);

  //copiar datos a nuestra pagina de envio (0x3FF000)
  //la pagina ya esta mapeada por initializeData
  copy_from_user(src, (void *)0x3FF000, size);

  //actualizar contador de bytes
  current()->mbox.sent_bytes_count = size;

  //desbloquear al lector si estaba bloqueado
  if (reader_task->state == ST_BLOCKED) {
    update_process_state_rr(reader_task, &readyqueue);
  }

  //bloquearnos en la lista de escritores del lector
  update_process_state_rr(current(), &reader_task->mbox.writer_list);

  //cedemos CPU usando el scheduler
  sched_next_rr();

  //readFrom nos "despierta" aqui. devolvemos los bytes escritos
  return size;
}

int sys_readFrom(int *pid, char *dst) {
  
  //si no hay escritores, bloqueamos el proceso lector
  if (list_empty(&current()->mbox.writer_list)) {
    //nos bloqueamos en la cola global 'blocked'
    update_process_state_rr(current(), &blocked);
    sched_next_rr();
    //al ser despertados por un writeTo, continuamos aqui
  }

  //hay un escritor. cogemos el primero de la lista.
  struct list_head *lh = list_first(&current()->mbox.writer_list);
  list_del(lh);
  struct task_struct *writer_task = list_head_to_task_struct(lh);

  //mapeamos la página del escritor en 0x3FD000
  mapPhysicalPageFrom(writer_task, 0x3FD000);

  //copiamos los datos al destino del usuario
  int bytes_read = writer_task->mbox.sent_bytes_count;
  copy_to_user((void *)0x3FD000, dst, bytes_read);

  //desmapeamos la página 0x3FD000
  unmapPage(current(), 0x3FD000);
  
  //flush de la TLB
  set_cr3(get_DIR(current()));

  //devolvemos el PID del escritor
  copy_to_user(&writer_task->PID, pid, sizeof(int));

  //desbloqueamos al escritor (ponerlo en la readyqueue)
  update_process_state_rr(writer_task, &readyqueue);

  //devolvemos bytes leídos
  return bytes_read;
}

//mapea el send_frame del proceso p en la logical_address
//del espacio de direcciones del proceso current()
void mapPhysicalPageFrom(struct task_struct *p, unsigned int logical_address) {
  if (p == NULL) return;

  //obtenemos la tabla de paginas del proceso actual
  page_table_entry *current_pt = get_PT(current());
  
  //obtenemos el frame fisico a mapear (la send_page del escritor)
  int physical_frame = p->mbox.send_frame;
  
  //calculamos la pagina logica de destino (ej: 0x3FD000 >> 12 = 0x3FD)
  unsigned int logical_page = logical_address >> 12;

  //realizamos el mapeo
  set_ss_pag(current_pt, logical_page, physical_frame);
}

//desmapea la 'logical_address' del espacio de direcciones del proceso p
void unmapPage(struct task_struct *p, unsigned int logical_address) {
  if (p == NULL) return;
  
  page_table_entry *pt = get_PT(p);
  unsigned int logical_page = logical_address >> 12;

  del_ss_pag(pt, logical_page);
}

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

  initializeData(&uchild->task);

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
  
  //liberamos frame de la send_page
  free_frame(current()->mbox.send_frame);
  //desmapeamos la página logica (0x3FF)
  del_ss_pag(process_PT, 0x3FF);

  //desbloqueamos a todos los escritores pendientes
  while (!list_empty(&current()->mbox.writer_list)) {
    struct list_head *lh = list_first(&current()->mbox.writer_list);
    list_del(lh);
    struct task_struct *writer_task = list_head_to_task_struct(lh);
    update_process_state_rr(writer_task, &readyqueue);
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
