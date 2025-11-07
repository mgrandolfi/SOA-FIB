#include <libc.h>

char buff[24];

int pid;

static int uitoa(unsigned n, char *buf, int buflen)
{
	if (buflen <= 1) {
    if (buflen == 1)
      buf[0] = 0;
    return 0;
  }
	if (n == 0) {
    buf[0] = '0';
    buf[1] = 0;
    return 1;
  }
	char t[12];
  int i=0;
	while (n && i<(int)sizeof(t)) {
    t[i++] = '0' + (n%10);
    n /= 10;
  }
	if (i >= buflen) i = buflen-1;
	for (int j = 0; j < i; ++j) buf[j] = t[i-1-j];
	buf[i] = 0;
  return i;
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
    /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

	//char* p = 0;
	//*p = 'x';

  	write(1, "[WRITE] hola\n", 13);

	// getpid
	int pid = getpid();
	char b[32];
	int len = uitoa((unsigned)pid, b, sizeof(b));
	write(1, "[GETPID] pid=", 13);
	write(1, b, len);

	// gettime
	/*write(1, "[GETTIME]\n", 10);

	unsigned last_bucket = (unsigned)-1;
	char num[16];

	for (volatile unsigned i = 0; i < 10000; ++i) {
		unsigned now = gettime();
		unsigned bucket = now / 5000;

		if (bucket != last_bucket) {
			int len = uitoa(now, num, sizeof(num));
			write(1, "[TICKS] ", 9);
			write(1, num, len);
			write(1, "\n", 1);
			last_bucket = bucket;

			for (volatile unsigned i=0; i<1000; ++i) { }
			for (volatile unsigned i=0; i<1000; ++i) { }
		}
	}*/

	write(1, "=== ZEOS DEMO KEYS ===\n", 23);
	write(1, "t: toggle logs task_switch\n", 27);
	write(1, "i: force switch init<->idle\n", 28);
	write(1, "p: print current pid\n", 21);
	write(1, "=======================\n", 23);

	while (1) {}
}