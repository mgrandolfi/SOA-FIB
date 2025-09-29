/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#define LECTURA 0
#define ESCRIPTURA 1

#define EBADF   9
#define EFAULT  14
#define EINVAL  22

extern int zeos_ticks;

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_write_console(char *buffer, int size);

int sys_write(int fd, char *buffer, int size)
{
    /* 1) validate parameters */
    int err = check_fd(fd, ESCRIPTURA);
    if (err < 0) return err;

    if (buffer == 0)    return -EFAULT;
    if (size   < 0)     return -EINVAL;
    if (size   == 0)    return 0;

    /* 2) copy from user to a kernel buffer and 3) do the write */
    int done = 0;
    char kbuf[256];                     /* chunked copy to avoid big stack frames */
    while (done < size) {
        int n = size - done;
        if (n > (int)sizeof(kbuf)) n = sizeof(kbuf);

        /* copy_from_user(user_src, kernel_dst, n) returns 0 on OK, <0 on error */
        if (copy_from_user(buffer + done, kbuf, n) < 0) return -EFAULT;

        sys_write_console(kbuf, n);
        done += n;
    }

    /* 4) return number of bytes written */
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

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
}
