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

int sys_fork(void)
{
  struct task_struct *parent = current();

  // a) get a free task_struct
  if (list_empty(&freequeue)) return -EAGAIN;
  struct list_head *lh = list_first(&freequeue);
  list_del(lh);
  struct task_struct *child = list_head_to_task_struct(lh);

  union task_union *u_parent = (union task_union*)parent;
  union task_union *u_child  = (union task_union*)child;

  // b) inherit system data: copy full task_union (PCB + kernel stack)
  copy_data(u_parent, u_child, sizeof(union task_union));

  // c) new page directory for child
  allocate_DIR(child);

  // d) allocate frames for child user data+stack
  int frames[NUM_PAG_DATA];
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    int f = alloc_frame();
    if (f < 0) {
      for (int j = 0; j < i; ++j) free_frame(frames[j]);
      list_add(&child->list, &freequeue);
      return -ENOMEM;
    }
    frames[i] = f;
  }

  // e) initialize child's address space
  page_table_entry *pt_parent = get_PT(parent);
  page_table_entry *pt_child  = get_PT(child);

  //   e.i) share system and user code pages with parent
  for (int i = 0; i < NUM_PAG_KERNEL; ++i) pt_child[i] = pt_parent[i];
  for (int i = 0; i < NUM_PAG_CODE;   ++i)
    pt_child[PAG_LOG_INIT_CODE + i] = pt_parent[PAG_LOG_INIT_CODE + i];

  //   e.ii) map child’s user data+stack to newly allocated frames
  for (int i = 0; i < NUM_PAG_DATA; ++i)
    set_ss_pag(pt_child, PAG_LOG_INIT_DATA + i, frames[i]);

  // f) inherit user data: temporary mappings in parent, copy, remove, flush TLB
  unsigned tmp = PAG_LOG_INIT_CODE + NUM_PAG_CODE; // free logical window after code
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    set_ss_pag(pt_parent, tmp + i, frames[i]);
    void *src = (void*)(L_USER_START + i*PAGE_SIZE);
    void *dst = (void*)((tmp + i) * PAGE_SIZE);
    copy_data(src, dst, PAGE_SIZE);
    del_ss_pag(pt_parent, tmp + i);
  }
  set_cr3(get_DIR(parent));  // flush parent TLB after deleting temps

  // g) assign PID
  static int next_pid = 2;   // 0 idle, 1 init
  child->PID = next_pid++;

  // h) fields not common to the child
  child->state = ST_READY;

  // i) prepare child stack so a task_switch returns via the syscall epilogue
  //    Switch code will set ESP = child->kernel_esp; then "pop %ebp; ret".
  //    Make that RET go to ret_from_fork(), which returns 0 in %eax.
  unsigned long *kesp = (unsigned long*)(u_child->task.kernel_esp);
  kesp[1] = (unsigned long)ret_from_fork;  // return address after saved EBP

  // j) enqueue child
  list_add_tail(&child->list, &readyqueue);

  // k) return child's pid to the parent
  return child->PID;
}

// Child continues here after the first schedule into it
int ret_from_fork(void)
{
  // Return 0 in %eax to the syscall epilogue
  return 0;
}

void sys_exit()
{  
  
}