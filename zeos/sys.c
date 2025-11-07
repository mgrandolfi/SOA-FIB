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
extern struct list_head readyqueue;
extern struct list_head blocked;
extern struct task_struct *idle_task;

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

  // b) inherit system data (PCB + kernel stack)
  copy_data(u_parent, u_child, sizeof(union task_union));

  // c) new page directory for child
  allocate_DIR(child);

  // d) allocate frames for child user data+stack
  int frames[NUM_PAG_DATA];
  int i;
  for (i = 0; i < NUM_PAG_DATA; ++i) {
    int f = alloc_frame();
    if (f < 0) {
      while (--i >= 0) free_frame(frames[i]);
      list_add_tail(&child->list, &freequeue);
      return -ENOMEM;
    }
    frames[i] = f;
  }

  // e) init child's address space
  page_table_entry *pt_parent = get_PT(parent);
  page_table_entry *pt_child  = get_PT(child);

  //   e.i) share kernel & user code
  for (i = 0; i < NUM_PAG_KERNEL; ++i) pt_child[i] = pt_parent[i];
  for (i = 0; i < NUM_PAG_CODE;   ++i)
    pt_child[PAG_LOG_INIT_CODE + i] = pt_parent[PAG_LOG_INIT_CODE + i];

  //   e.ii) map data+stack to new frames
  for (i = 0; i < NUM_PAG_DATA; ++i)
    set_ss_pag(pt_child, PAG_LOG_INIT_DATA + i, frames[i]);

  // f) copy parent data+stack -> child frames via a temp window in parent's PT
  int tmp = PAG_LOG_INIT_CODE + NUM_PAG_CODE; // free window after user code
  for (i = 0; i < NUM_PAG_DATA; ++i) {
    set_ss_pag(pt_parent, tmp + i, frames[i]);
    copy_data((void*)(L_USER_START + i*PAGE_SIZE),
              (void*)((tmp + i) << 12),
              PAGE_SIZE);
    del_ss_pag(pt_parent, tmp + i);
  }
  //   (flush TLB after removing temp mappings)
  set_cr3(get_DIR(parent));  // f.C: really disable parent's access to child pages. :contentReference[oaicite:3]{index=3}

  // g) assign PID (distinct from task array position)
  static int next_pid = 2;
  child->PID = next_pid++;

  // h+i) prepare child kernel stack so a task_switch resumes in user mode
  // Rebase the saved kernel_esp from parent's stack to child's stack
  unsigned long parent_off =
      (unsigned long)u_parent->task.kernel_esp - (unsigned long)u_parent->stack;
  unsigned long *child_kesp =
      (unsigned long*)((unsigned long)u_child->stack + parent_off);

  u_child->task.kernel_esp = (unsigned long)child_kesp;

  child_kesp[1] = (unsigned long)ret_from_fork;

  child->parent = parent;
  INIT_LIST_HEAD(&child->children);
  INIT_LIST_HEAD(&child->sibling);
  child->pending_unblocks = 0;
  list_add_tail(&child->sibling, &parent->children);

  // j) enqueue child as ready
  child->state = ST_READY;
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
  struct task_struct *p = current();

  if (p->parent) list_del(&p->sibling);

  if (p->PID == 1) {
    printk("\nThe task 1 cannot exit\n");
    return;
  }

  struct list_head *pos, *n;
  list_for_each_safe(pos, n, &p->children) {
    struct task_struct *ch = list_entry(pos, struct task_struct, sibling);
    list_del(pos);
    ch->parent = idle_task;
    list_add_tail(&ch->sibling, &idle_task->children);
  }

  page_table_entry *pt = get_PT(p);
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    unsigned idx = PAG_LOG_INIT_DATA + i;
    if (pt[idx].bits.present) {
      int frame = pt[idx].bits.pbase_addr;  // frame number as stored in PT
      del_ss_pag(pt, idx);                  // remove mapping first
      free_frame(frame);                    // release physical frame
    }
  }
  set_cr3(get_DIR(p));                      // flush TLB for current PT

  // Return PCB to the free pool and switch to next runnable task
  update_process_state_rr(p, &freequeue);
  sched_next_rr();
}

int sys_block(void)
{
  struct task_struct *p = current();

  if (p->pending_unblocks > 0) {
    --p->pending_unblocks;
    return 0;
  }

  update_process_state_rr(p, &blocked);
  sched_next_rr();
  return 0;

}

static struct task_struct *find_child_by_pid(struct task_struct *parent, int pid)
{
  struct list_head *pos;
  list_for_each(pos, &parent->children) {
    struct task_struct *ch = list_entry(pos, struct task_struct, sibling);
    if (ch->PID == pid) return ch;
  }
  return 0;
}

int sys_unblock(int pid)
{
  struct task_struct *me = current();
  struct task_struct *ch = find_child_by_pid(me, pid);
  if (!ch) return -1;

  if (ch->state == ST_BLOCKED) {
    update_process_state_rr(ch, &readyqueue);
  } else {
    ++ch->pending_unblocks;
  }
  return 0;
}