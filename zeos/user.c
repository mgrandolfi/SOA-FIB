#include <libc.h>

char buff[24];

int pid;

extern int gettime();

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
    /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  //char* p = 0;
  //*p = 'x';

  write(1, "hello, zeos!\n", 13);
  write(1, "hello again!\n", 13);

  /*int time_val = gettime();
  itoa(time_val, buff);
  write(1, buff, strlen(buff));*/

  while(1) {
  }
}
