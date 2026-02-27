/*
 * io.c - 
 */

#include <io.h>

#include <types.h>

/**************/
/** Screen  ***/
/**************/

#define NUM_COLUMNS 80
#define NUM_ROWS    25

Byte x, y=19;

/* Read a byte from 'port' */
Byte inb (unsigned short port)
{
  Byte v;

  __asm__ __volatile__ ("inb %w1,%0":"=a" (v):"Nd" (port));
  return v;
}

void printc(char c)
{
     __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); /* Magic BOCHS debug: writes 'c' to port 0xe9 */
  if (c=='\n')
  {
    x = 0;
    y=(y+1)%NUM_ROWS;
  }
  else
  {
    Word ch = (Word) (c & 0x00FF) | 0x0200;
	Word *screen = (Word *)0xb8000;
	screen[(y * NUM_COLUMNS + x)] = ch;
    if (++x >= NUM_COLUMNS)
    {
      x = 0;
      y=(y+1)%NUM_ROWS;
    }
  }
}

void printc_xy(Byte mx, Byte my, char c)
{
  Byte cx, cy;
  cx=x;
  cy=y;
  x=mx;
  y=my;
  printc(c);
  x=cx;
  y=cy;
}

void printk(char *string)
{
  int i;
  for (i = 0; string[i]; i++)
    printc(string[i]);
}

/*
 * printk_hex - Imprimeix un nombre en format hexadecimal
 * @num: el número a imprimir
 */
void printk_hex(unsigned int num)
{
  char hex_chars[] = "0123456789ABCDEF";
  char buffer[9]; // 8 hex digits + null terminator
  int i;
  
  for (i = 7; i >= 0; i--) {
    buffer[i] = hex_chars[num & 0xF];
    num >>= 4;
  }
  buffer[8] = '\0';
  
  printk(buffer);
}

/*
 * printk_int - Imprimeix un nombre enter en format decimal
 * @num: el número a imprimir
 */
void printk_int(int num)
{
  char buffer[12]; // Suficient per a un enter de 32 bits amb signe
  int i = 0;
  int is_negative = 0;
  
  if (num == 0) {
    printc('0');
    return;
  }
  
  if (num < 0) {
    is_negative = 1;
    num = -num;
  }
  
  while (num > 0) {
    buffer[i++] = '0' + (num % 10);
    num /= 10;
  }
  
  if (is_negative) {
    printc('-');
  }
  
  // Imprimeix els digits en ordre invers
  while (i > 0) {
    printc(buffer[--i]);
  }
}

