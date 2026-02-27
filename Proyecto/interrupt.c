/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <utils.h>

#include <sched.h>

#include <zeos_interrupt.h>

// Declaració de funcions externes
extern void sys_exit(void);

Gate idt[IDT_ENTRIES];
Register    idtR;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

int zeos_ticks = 0;

// Variable global per indicar si hi ha un callback de teclat en execució
int keyboard_callback_active = 0;

void clock_routine()
{
  // zeos_show_clock(); // Desactivat per al videojoc - els FPS es mostren al renderitzador
  zeos_ticks ++;
  
  // Despertem tots els threads bloquejats esperant el tick
  while (!list_empty(&blocked_tick_queue)) {
    struct list_head *e = list_first(&blocked_tick_queue);
    struct task_struct *t = list_head_to_task_struct(e);
    update_process_state_rr(t, &readyqueue);
  }

  schedule();
}

void keyboard_routine(struct pt_regs *regs)
{
  unsigned char c = inb(0x60);
  struct task_struct *t = current();
  
  int pressed = (c & 0x80) ? 0 : 1;
  char key = c & 0x7F;

  // Si ja hi ha un callback actiu, ignorem l'event
  if (keyboard_callback_active) return;
  
  // Si hi ha callback registrat i no estem ja executant-lo
  if (t->keyboard_wrapper && !t->in_keyboard_callback) {
    t->saved_regs = *regs;       // Guardem context original
    t->in_keyboard_callback = 1; // Marquem que estem en callback
    keyboard_callback_active = 1; // Marquem globalment que hi ha callback actiu
    
    // Preparem la pila auxiliar
    unsigned int *stack = (unsigned int *)t->keyboard_aux_stack;
    
    stack--;
    *stack = pressed;            // Segon argument: pressed
    stack--;
    *stack = (unsigned int)key;  // Primer argument: key
    
    // Modifiquem context per saltar al wrapper
    regs->esp = (unsigned long)stack;
    regs->eip = (unsigned long)t->keyboard_wrapper;
    regs->eax = (unsigned long)t->keyboard_callback; // Passem callback a %eax
  }
}

void keyboard_exit_routine(struct pt_regs *regs) {
  struct task_struct *t = current();
  // Només fem res si realment estem en un callback de teclat
  // Si int 0x2b es crida fora del keyboard support, no fa res
  if (t->in_keyboard_callback) {
    t->in_keyboard_callback = 0;    // Sortim del mode callback
    keyboard_callback_active = 0;   // Marquem globalment que no hi ha callback actiu
    *regs = t->saved_regs;          // Restaurem context original
  }
}

void recover_page_fault()
{
  return;
}

void page_fault_routine_custom()
{
  unsigned int faulting_address;
  struct task_struct *current_task;
  
  // Obtenim l'adreça que ha causat el page fault
  faulting_address = get_cr2();
  
  current_task = current();
  
  // DEBUG: Descomentar per veure info del page fault
  /*
  printk("\nPF: addr=0x");
  printk_hex(faulting_address);
  printk(" TID=");
  printk_int(current_task->TID);
  */
  
  // Mirem si el fault és per accedir al gap abans de la pila d'usuari
  if (is_stack_growth_fault(faulting_address, current_task) && grow_user_stack(current_task, faulting_address) == 0) {
    recover_page_fault();
  }
  else {
    printk("\nPAGE FAULT at address: 0x");
    printk_hex(faulting_address);
    printk("\n");
    printk("Process PID: ");
    printk_int(current_task->PID);
    printk(", TID: ");
    printk_int(current_task->TID);
    printk("\n");

    sys_exit();
  }
}



void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void clock_handler();
void keyboard_handler();
void page_fault_handler_custom();
void system_call_handler();
void keyboard_exit_handler();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(14, page_fault_handler_custom, 0); // Page Fault Exception
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);
  
  setTrapHandler(0x2b, keyboard_exit_handler, 3);

  setSysenter();

  set_idt_reg(&idtR);
}

