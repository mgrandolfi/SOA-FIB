#include <libc.h>

char buff[24];

int pid;

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
    /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  /*char* p = 0;
  *p = 'x';*/

  write(1, "hello, zeos!\n", 13);      /* stdout */
  write(2, "and to stderr!\n", 15);    /* stderr */

  while(1) { }
}
