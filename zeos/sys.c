/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#include <interrupt.h>

#define LECTURA 0
#define ESCRIPTURA 1

extern int zeos_ticks;

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_write(int fd, char *buffer, int size)
{
    // fem check dels paràmetres
    int err = check_fd(fd, ESCRIPTURA);
    if (err < 0) return err;

    if (buffer == 0)    return -EFAULT;
    if (size   < 0)     return -EINVAL;
    if (size   == 0)    return 0;

    // copiem de user a un kernel buffer i fem el write
    int done = 0;
    char kbuf[256];
    while (done < size) {
        int n = size - done;
        if (n > (int)sizeof(kbuf)) n = sizeof(kbuf);

        // copy_from_user(user_src, kernel_dst, n) retorna 0 si OK, <0 si dona error */
        if (copy_from_user(buffer + done, kbuf, n) < 0) return -EFAULT;

        sys_write_console(kbuf, n);
        done += n;
    }

    // retornem número de bytes escrits
    return done;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_gettime() {
  return zeos_ticks;
}

int sys_getpid()
{
	return current()->PID;
}

extern struct list_head freequeue;

int sys_fork()
{

  int PID=-1;

  // creates the child process

  //if the freeque is empty means there is no space for a new process.
  if (list_empty(&freequeue)) return -ENOMEM;  // Theres no free PCB task_struct
  

  //Get a free task_struct for the process.
  struct list_head *free_child_head = list_first(&freequeue);
  list_del(free_child_head); //remove from freequeue, not usable anymore

  struct task_struct *child_task_struct = list_head_to_task_struct(free_child_head);
  union task_union *child_task_union = (union task_union *) child_task_struct; 

  //copy the parent’s task_union to the child. 
  //copy_data(void *start, void *dest, int size), every task_union has PAGE_SIZE size;
  copy_data(current(), child_task_union, PAGE_SIZE);

  //Get a new page directory to store the child with alocate_DIR()
  allocate_DIR(child_task_struct);

  //Search frames (physical pages) in which to map logical pages for data+stack of the child process
  int data_frame[NUM_PAG_DATA];
  for (int i = 0; i < NUM_PAG_DATA; i++) {
      int f = alloc_frame(); 
      if (f < 0) {
          //Free previously allocated frames (we make a rollback)
          for (int j = 0; j < i; j++) {
              free_frame(data_frame[j]);
          }
          //Return error no memory
          list_add(free_child_head, &freequeue); //add again to freequeue
          return -ENOMEM; //returns error no memory
      }
      data_frame[i] = f; //store allocated frame
  }

  page_table_entry* PTC = get_PT(child_task_struct);
  page_table_entry* PTP = get_PT(current());


  for (int pag = 0; pag < NUM_PAG_KERNEL; pag++) {
        set_ss_pag(PTC, pag, get_frame(PTP, pag));
  }

  //f) 
  for (int pag = PAG_LOG_INIT_DATA; pag < PAG_LOG_INIT_DATA+NUM_PAG_DATA; pag++) {
       
        void *parent_addr = (void*)(pag << 12); // take out the offset
        
        // temporal ubication 
        int temp_page = pag + NUM_PAG_DATA + NUM_PAG_CODE;
        void *temp_addr = (void*)(temp_page << 12);
        
        // 1. Crear mapeo temporal: hacer accesible la página física del hijo 
        //    desde el espacio de direcciones del padre
        unsigned int child_frame = get_frame(PTC, pag);
        set_ss_pag(PTP, temp_page, child_frame);
        
        // 2. Copiar los datos del padre al espacio del hijo
        copy_data(parent_addr, temp_addr, PAGE_SIZE);
        
        // 3. Eliminar el mapeo temporal
        del_ss_pag(PTP, temp_page);
    }

    //g) 
    child_task_struct->PID = get_next_pid(); 
    child_task_struct->parent = current(); 

    //h) initialize the fields of the task_struct that are not common to the child.
	  INIT_LIST_HEAD(&child_task_struct->list);

  return PID;
}

void sys_exit()
{  
}