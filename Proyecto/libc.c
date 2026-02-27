/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

#define MAX_THREADS 32

// Array per emmagatzemar l'errno de cada thread de forma independent
int errno_storage[MAX_THREADS];

// Retorna l'adreça de memòria de la variable errno corresponent al thread actual
int *__errno_location(void) {
  return &errno_storage[get_stack_id()];
}

int REGS[7]; // Space to save REGISTERS

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

void perror()
{
  char buffer[256];

  itoa(errno, buffer);

  write(1, buffer, strlen(buffer));
}