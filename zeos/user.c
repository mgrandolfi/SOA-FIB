#include <libc.h>

char buff[24];

int pid;

void test_gettime() {
    write(1, "\n\nTesting gettime() syscall...\n", 31);
    itoa(gettime(), buff);
    write(1, "Ticks: ", 7);
    write(1, buff, strlen(buff));
    write(1, "\ngettime() test: PASSED", 23);
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
    /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  //char* p = 0;
  //*p = 'x';

  write(1, "hello, zeos!\n", 13);
  write(1, "hello again!\n", 13);

  test_gettime();

  while(1) {
    int ticks = gettime();
    itoa(ticks, buff);
    write(1, "Ticks: ", 7);
    write(1, buff, strlen(buff));
    write(1, "\n", 1);
    for (int i = 0; i < 100000000; i++);
  }
}
