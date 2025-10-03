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

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
}
