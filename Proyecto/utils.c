#include <utils.h>
#include <types.h>
#include <sched.h>
#include <mm_address.h>

void copy_data(void *start, void *dest, int size)
{
  DWord *p = start, *q = dest;
  Byte *p1, *q1;
  while(size > 4) {
    *q++ = *p++;
    size -= 4;
  }
  p1=(Byte*)p;
  q1=(Byte*)q;
  while(size > 0) {
    *q1++ = *p1++;
    size --;
  }
}

int copy_from_user(void *start, void *dest, int size)
{
  DWord *p = start, *q = dest;
  Byte *p1, *q1;
  while(size > 4) {
    *q++ = *p++;
    size -= 4;
  }
  p1=(Byte*)p;
  q1=(Byte*)q;
  while(size > 0) {
    *q1++ = *p1++;
    size --;
  }
  return 0;
}

int copy_to_user(void *start, void *dest, int size)
{
  DWord *p = start, *q = dest;
  Byte *p1, *q1;
  while(size > 4) {
    *q++ = *p++;
    size -= 4;
  }
  p1=(Byte*)p;
  q1=(Byte*)q;
  while(size > 0) {
    *q1++ = *p1++;
    size --;
  }
  return 0;
}

// Per la comprovació de paràmetres que li passarem a la funció ThreadCreate
int access_ok(int type, const void * addr, unsigned long size)
{
  unsigned long addr_val = (unsigned long)addr;
  unsigned long limit = addr_val + size;

  // Comprovem Codi (Read-only)
  if (type == VERIFY_READ) {
    if (addr_val >= (USER_FIRST_PAGE * PAGE_SIZE) && limit <= ((USER_FIRST_PAGE + NUM_PAG_CODE) * PAGE_SIZE))
      return 1;
  }

  // Comprovem Dades (Read/Write)
  if (addr_val >= ((USER_FIRST_PAGE + NUM_PAG_CODE) * PAGE_SIZE) &&
    limit <= ((USER_FIRST_PAGE + NUM_PAG_CODE + NUM_PAG_DATA) * PAGE_SIZE))
    return 1;

  // Validem que l'adreça caigui dins de la regió total de piles de threads
  if (addr_val >= (FIRST_THREAD_STACK_PAGE * PAGE_SIZE) &&
    limit <= ((FIRST_THREAD_STACK_PAGE + (MAX_THREADS * (THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES))) * PAGE_SIZE))
  {
    struct task_struct *t = current();
    struct task_struct *leader = t->process_leader;
    
    // Calculem a quin stack_id pertany l'adreça
    // La fórmula ha de coincidir amb allocate_thread_stack()
    unsigned int addr_page = addr_val / PAGE_SIZE;
    int target_stack_id = (addr_page - FIRST_THREAD_STACK_PAGE) / (THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES);
    
    // Verifiquem que el thread amb aquest stack_id existeixi
    if (target_stack_id >= 0 && target_stack_id < MAX_THREADS) {
      // Comprovem si el bit corresponent està activat al threads_mask del líder
      if (leader->threads_mask & (1 << target_stack_id)) {
        // El thread existeix, permetem accés a TOTA la seva regió
        unsigned long stack_start = (FIRST_THREAD_STACK_PAGE + target_stack_id * (THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES)) * PAGE_SIZE;
        unsigned long stack_end = (FIRST_THREAD_STACK_PAGE + (target_stack_id + 1) * (THREAD_STACK_GAP_PAGES + THREAD_STACK_MAX_PAGES)) * PAGE_SIZE;
        
        if (addr_val >= stack_start && limit <= stack_end) {
          return 1;
        }
      }
    }
    // Si cau en la regió de piles però el thread no existeix, deneguem accés
    return 0;
  }
  return 0;
}


#define CYCLESPERTICK 109000


#define do_div(n,base) ({ \
        unsigned long __upper, __low, __high, __mod, __base; \
        __base = (base); \
        asm("":"=a" (__low), "=d" (__high):"A" (n)); \
        __upper = __high; \
        if (__high) { \
                __upper = __high % (__base); \
                __high = __high / (__base); \
        } \
        asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
        asm("":"=A" (n):"a" (__low),"d" (__high)); \
        __mod; \
})


#define rdtsc(low,high) \
        __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

unsigned long get_ticks(void) {
        unsigned long eax;
        unsigned long edx;
        unsigned long long ticks;

        rdtsc(eax,edx);

        ticks=((unsigned long long) edx << 32) + eax;
        do_div(ticks,CYCLESPERTICK);

        return ticks;
}

void memset(void *s, unsigned char c, int size)
{
  unsigned char *m=(unsigned char *)s;
  
  int i;
  
  for (i=0; i<size; i++)
  {
    m[i]=c;
  }
}
