#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define ENTRY_DIR_PAGES       0

#define TOTAL_PAGES 1024
#define NUM_PAG_KERNEL 256
#define PAG_LOG_INIT_CODE (L_USER_START>>12)
#define FRAME_INIT_CODE (PH_USER_START>>12)
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (PAG_LOG_INIT_CODE+NUM_PAG_CODE)
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000

// Variables per a la gestió de les piles dels THREADS
// Cada thread stack té un gap per permetre el creixement dinàmic
#define THREAD_STACK_INITIAL_PAGES 1  // Mida inicial de la pila d'usuari per un thread: 1 pàgina
#define THREAD_STACK_GAP_PAGES 10     // Mida del gap abans de cada pila de thread per al creixement
#define THREAD_STACK_MAX_PAGES (THREAD_STACK_INITIAL_PAGES + THREAD_STACK_GAP_PAGES) // Pàgines màximes per thread
// El primer thread (main thread) usa l'àrea DATA estàndard
// Threads addicionals comencen a assignar les seves piles a partir d'aquesta pàgina
#define FIRST_THREAD_STACK_PAGE (PAG_LOG_INIT_DATA + NUM_PAG_DATA)
#define MAX_THREADS 32

/* Memory distribution */
/***********************/


#define KERNEL_START     0x10000
#define L_USER_START        0x100000
#define PH_USER_START       0x100000
#define USER_ESP	L_USER_START+(NUM_PAG_CODE+NUM_PAG_DATA)*0x1000-16

#define USER_FIRST_PAGE	(L_USER_START>>12)

#define PH_PAGE(x) (x>>12)

#endif

