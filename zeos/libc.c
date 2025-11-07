/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

#include <errno.h>

int errno;

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}

void perror(void){
  switch (errno)
  {
    // check_fd errors
    case EBADF:
      write(1, "Bad file number\n", 16);
      break;
    
    case EACCES:  
      write(1, "Permission denied\n", 18);
      break;

    // sys_write errors
    case EFAULT:
      write(1, "Bad address\n", 12);
      break;
    
    case EINVAL:
      write(1, "Invalid argument\n", 17);
      break;

    // sys_ni_syscall error
    case ENOSYS:
      write(1, "Invalid system call number\n", 25);
      break;
    
    // "user" error quan es retorna -1
    default:
      write(1, "Operation not permitted\n", 23);
      break;
  }
}

