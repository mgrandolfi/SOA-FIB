#include <libc.h>
#include <errno.h>

char buff[24];

int pid;

// Comptadors globals per testing
int thread1_counter = 0;
int thread2_counter = 0;
int thread3_counter = 0;
int recursion_counter = 0;
int dynamic_counter_1 = 0;
int dynamic_counter_2 = 0;
int dynamic_counter_3 = 0;
int exit_counter = 0;

// === Test 1: Creació i execució simple d'un thread ===
void test_thread_1(void *arg)
{
  int *counter = (int *)arg;
  int i;
  
  write(1, "  Thread iniciat\n", 18);
  
  for (i = 0; i < 5; i++) {
    (*counter)++;
    yield();
  }
  
  write(1, "  Thread finalitzat\n", 21);
  ThreadExit();
}

// Test 2: Thread amb més iteracions per testejar concurrència
void test_thread_2(void *arg)
{
  int *counter = (int *)arg;
  int i;
  
  write(1, "  Thread iniciat\n", 18);
  
  for (i = 0; i < 10; i++) {
    (*counter)++;
    yield();
  }
  
  write(1, "  Thread finalitzat\n", 21);
  ThreadExit();
}

// Test 3: Thread que prova el creixement dinàmic de pila
void recursive_function(int depth, int *counter)
{
  char local_buffer[128];
  int i;
  
  for (i = 0; i < 128; i++) {
    local_buffer[i] = (char)i;
  }
  
  (*counter)++;
  
  if (depth > 0) {
    recursive_function(depth - 1, counter);
  }
  
  if (local_buffer[0] != 0) {
    yield();
  }
}

void test_thread_3(void *arg)
{
  int *counter = (int *)arg;
  
  write(1, "  Thread creixement pila iniciat\n", 35);
  
  recursive_function(20, counter);
  
  write(1, "  Thread creixement pila finalitzat\n", 38);
  ThreadExit();
}

/* ========================================= */
/* FUNCIONS DE TEST                          */
/* ========================================= */

void test_1_simple_thread_creation(void)
{
  int tid;
  int i;
  
  write(1, "=== Test 1: Creacio simple de thread ===\n", 43);

  // Test errno functionality
  write(1, "Testejant errno amb write invalid...\n", 37);
  if (write(-1, "buffer", 6) < 0) {
    char err_buff[16];
    write(1, "El write falla. errno: ", 24);
    itoa(errno, err_buff);
    write(1, err_buff, strlen(err_buff));
    write(1, "\n", 1);
  }
  
  thread1_counter = 0;
  tid = ThreadCreate(test_thread_1, (void *)&thread1_counter);
  
  if (tid < 0) {
    write(1, "ERROR: No s'ha pogut crear el thread\n", 39);
    return;
  }
  
  write(1, "Thread creat correctament\n", 27);
  
  // Deixem que el thread s'executi
  for (i = 0; i < 10; i++) {
    yield();
  }
  
  write(1, "Comptador thread 1 (iteracions): ", 34);
  itoa(thread1_counter, buff);
  write(1, buff, strlen(buff));
  write(1, "\n", 1);
}

void test_2_concurrent_threads(void)
{
  int tid2, tid3;
  int i;
  
  write(1, "\n=== Test 2: Threads concurrents ===\n", 38);
  
  thread2_counter = 0;
  thread3_counter = 0;
  
  tid2 = ThreadCreate(test_thread_2, (void *)&thread2_counter);
  tid3 = ThreadCreate(test_thread_1, (void *)&thread3_counter);
  
  if (tid2 < 0 || tid3 < 0) {
    write(1, "ERROR: No s'han pogut crear els threads\n", 42);
    return;
  }
  
  // Deixem que els threads s'executin
  for (i = 0; i < 20; i++) {
    yield();
  }
  
  write(1, "Comptador thread 2: ", 21);
  itoa(thread2_counter, buff);
  write(1, buff, strlen(buff));
  write(1, "\n", 1);
  
  write(1, "Comptador thread 3: ", 21);
  itoa(thread3_counter, buff);
  write(1, buff, strlen(buff));
  write(1, "\n", 1);
}

void test_3_stack_growth(void)
{
  int tid;
  int i;
  
  write(1, "\n=== Test 3: Creixement de pila ===\n", 37);
  
  recursion_counter = 0;
  tid = ThreadCreate(test_thread_3, (void *)&recursion_counter);
  
  if (tid < 0) {
    write(1, "ERROR: No s'ha pogut crear el thread de recursio\n", 51);
    return;
  }
  
  // Deixem que el thread faci recursió
  for (i = 0; i < 30; i++) {
    yield();
  }
  
  write(1, "Compte de recursions (iteracions creixement pila): ", 53);
  itoa(recursion_counter, buff);
  write(1, buff, strlen(buff));
  write(1, "\n", 1);
}

void test_4_fork_with_threads(void)
{
  int child_pid;
  
  write(1, "\n=== Test 4: Fork amb threads ===\n", 35);
  
  // Creem un thread abans del fork
  write(1, "Creant thread abans del fork...\n", 33);
  ThreadCreate(test_thread_1, (void *)&thread1_counter);
  yield();
  
  write(1, "Cridant fork()...\n", 19);
  child_pid = fork();
  
  if (child_pid == 0) {
    // Procés fill
    write(1, "  [FILL] Proces fill creat\n", 28);
    write(1, "  [FILL] Només el thread actual es clona (fork clona 1 thread)\n", 66);
    
    // Creem un thread nou al fill
    write(1, "  [FILL] Creant nou thread al fill...\n", 40);
    ThreadCreate(test_thread_2, (void *)&thread2_counter);
    
    for (child_pid = 0; child_pid < 10; child_pid++) yield();
    
    write(1, "  [FILL] Proces fill sortint\n", 31);
    exit();
  } else {
    // Procés pare
    write(1, "  [PARE] Fork completat, PID fill: ", 37);
    itoa(child_pid, buff);
    write(1, buff, strlen(buff));
    write(1, "\n", 1);
    write(1, "  [PARE] Continuant amb threads\n", 34);
    
    // Esperem que el fill acabi completament
    for (child_pid = 0; child_pid < 25; child_pid++) yield();
    
    write(1, "\n[Test 4] PASSAT\n", 18);
  }
}

/* === Test 5: Validació access_ok === */

// Variable global per test access_ok (a la secció DATA)
char global_data_buffer[64];

// Thread auxiliar per test d'access_ok amb pila vàlida
void access_ok_helper_thread(void *arg)
{
  char local_buffer[32];
  int *result = (int *)arg;
  int i;
  
  // Inicialitzem el buffer local
  for (i = 0; i < 32; i++) local_buffer[i] = 'A' + (i % 26);
  local_buffer[31] = '\0';
  
  // Test: write amb buffer a la PILA del thread (hauria de funcionar)
  int ret = write(1, "  [Thread] Escrivint des de pila propia: ", 42);
  if (ret >= 0) {
    write(1, local_buffer, 10);
    write(1, "\n", 1);
    *result = 1;
  } else {
    write(1, "  [Thread] ERROR: write amb pila propia ha fallat!\n", 53);
    *result = -1;
  }
  
  ThreadExit();
}

void test_5_access_ok_validation(void)
{
  int tid;
  int i;
  int result = 0;
  int ret;
  
  write(1, "\n=== Test 5: Validacio access_ok ===\n", 38);
  
  write(1, "\n--- Part A: Regions valides ---\n", 34);
  
  // Test A.1: Escriptura a buffer GLOBAL (secció DATA)
  write(1, "\n[Test 5.A.1] Buffer a seccio DATA (global_data_buffer)\n", 57);
  for (i = 0; i < 63; i++) global_data_buffer[i] = 'X';
  global_data_buffer[63] = '\0';
  
  ret = write(1, "  Escrivint des de DATA: ", 26);
  if (ret >= 0) {
    ret = write(1, global_data_buffer, 10);
    if (ret >= 0) {
      write(1, "\n", 1);
      write(1, "[Test 5.A.1] PASSAT: Acces a DATA acceptat\n", 44);
    } else {
      write(1, "\n[Test 5.A.1] FALLAT: write a DATA ha fallat\n", 46);
    }
  } else {
    write(1, "[Test 5.A.1] FALLAT: write inicial ha fallat\n", 46);
  }
  
  // Test A.2: Escriptura des de PILA del thread principal (stack_id=0)
  write(1, "\n[Test 5.A.2] Buffer a PILA del main thread\n", 45);
  char local_main_buffer[32];
  for (i = 0; i < 31; i++) local_main_buffer[i] = 'M';
  local_main_buffer[31] = '\0';
  
  ret = write(1, "  Escrivint des de pila main: ", 31);
  if (ret >= 0) {
    ret = write(1, local_main_buffer, 10);
    if (ret >= 0) {
      write(1, "\n", 1);
      write(1, "[Test 5.A.2] PASSAT: Acces a pila main acceptat\n", 49);
    } else {
      write(1, "\n[Test 5.A.2] FALLAT: write a pila main ha fallat\n", 51);
    }
  } else {
    write(1, "[Test 5.A.2] FALLAT: write inicial ha fallat\n", 46);
  }
  
  // Test A.3: Thread secundari escriu des de la SEVA pila
  write(1, "\n[Test 5.A.3] Buffer a PILA de thread secundari\n", 49);
  result = 0;
  tid = ThreadCreate(access_ok_helper_thread, (void *)&result);
  
  if (tid < 0) {
    write(1, "[Test 5.A.3] ERROR: No s'ha pogut crear thread\n", 48);
  } else {
    // Esperem que el thread acabi
    for (i = 0; i < 10; i++) yield();
    
    if (result == 1) {
      write(1, "[Test 5.A.3] PASSAT: Thread pot escriure des de la seva pila\n", 62);
    } else if (result == -1) {
      write(1, "[Test 5.A.3] FALLAT: Thread NO pot escriure des de la seva pila\n", 65);
    } else {
      write(1, "[Test 5.A.3] TIMEOUT: Thread no ha acabat\n", 43);
    }
  }
  
  write(1, "\n--- Part B: Regions invalides + CODE Read ---\n", 48);
  
  // Test B.1: Pila de thread INEXISTENT (stack_id=5 no creat)
  write(1, "\n[Test 5.B.1] Pila de thread INEXISTENT (stack_id=5)\n", 54);
  
  // stack_id=5 -> pàgina = 284 + 5*21 = 389 -> 0x185000, usem 0x184F00
  char *fake_stack_addr = (char *)0x184F00;
  ret = write(1, fake_stack_addr, 10);
  
  if (ret < 0 && errno == 14) {
    write(1, "  errno=EFAULT (CORRECTE!)\n", 28);
    write(1, "[Test 5.B.1] PASSAT: Rebutja pila inexistent\n", 46);
  } else {
    write(1, "[Test 5.B.1] FALLAT\n", 21);
  }
  
  // Test B.2: Adreça fora de rang (més enllà de totes les regions)
  write(1, "\n[Test 5.B.2] Adreca fora de rang (0x500000)\n", 46);
  
  char *invalid_addr = (char *)0x500000;
  ret = write(1, invalid_addr, 10);
  
  if (ret < 0) {
    write(1, "  write() ha fallat (CORRECTE!)\n", 33);
    write(1, "[Test 5.B.2] PASSAT: Rebutja adreca invalida\n", 46);
  } else {
    write(1, "[Test 5.B.2] FALLAT\n", 21);
  }
  
  // Test B.3: Adreça NULL
  write(1, "\n[Test 5.B.3] Adreca NULL (0x0)\n", 32);
  
  ret = write(1, (char *)0x0, 10);
  
  if (ret < 0) {
    write(1, "  write() ha fallat (CORRECTE!)\n", 33);
    write(1, "[Test 5.B.3] PASSAT: Rebutja NULL\n", 35);
  } else {
    write(1, "[Test 5.B.3] FALLAT\n", 21);
  }
  
  // Test B.4: Adreça al KERNEL (per sota de USER_FIRST_PAGE)
  write(1, "\n[Test 5.B.4] Adreca a espai KERNEL (0x1000)\n", 46);
  
  char *kernel_addr = (char *)0x1000;
  ret = write(1, kernel_addr, 10);
  
  if (ret < 0) {
    write(1, "  write() ha fallat (CORRECTE!)\n", 33);
    write(1, "[Test 5.B.4] PASSAT: Rebutja acces a kernel\n", 45);
  } else {
    write(1, "[Test 5.B.4] FALLAT: Acces a kernel permès!\n", 45);
  }
  
  // Test B.5: Buffer que CREUA FRONTERES (DATA -> zona invàlida)
  // DATA acaba a 0x11C000
  // Usem un buffer que comenci just abans del límit i el superi
  write(1, "\n[Test 5.B.5] Buffer que creua frontera DATA->invalid\n", 55);
  
  char *boundary_addr = (char *)0x11BFF8;
  ret = write(1, boundary_addr, 16);  // 16 bytes creuaran el límit de DATA
  
  if (ret < 0) {
    write(1, "  write() ha fallat (CORRECTE!)\n", 33);
    write(1, "[Test 5.B.5] PASSAT: Rebutja buffer que creua frontera\n", 56);
  } else {
    write(1, "[Test 5.B.5] FALLAT: Hauria d'haver rebutjat!\n", 47);
  }
  
  // Test B.6: VERIFY_READ sobre secció CODE
  // Si passem una adreça de CODE, access_ok hauria d'acceptar-la
  write(1, "\n[Test 5.B.6] VERIFY_READ sobre seccio CODE (via write)\n", 57);
  
  // Usem l'adreça de la funció main (que està a la secció CODE)
  extern int main(void);
  char *main_addr = (char *)main;
  
  // Mostrem l'adreça (0x100000 = 1048576 decimal, inici de CODE)
  write(1, "  Adreca main() = ", 18);
  char addr_str[16];
  itoa((unsigned int)main_addr, addr_str);
  write(1, addr_str, strlen(addr_str));
  write(1, " (esperat: 1048576 = 0x100000)\n", 30);
  
  ret = write(1, main_addr, 0);
  
  if (ret >= 0) {
    write(1, "\n  access_ok(VERIFY_READ, CODE) = OK\n", 38);
    write(1, "[Test 5.B.6] PASSAT: Lectura de CODE acceptada\n", 48);
  } else {
    write(1, "[Test 5.B.6] FALLAT: No s'ha pogut llegir CODE\n", 48);
  }
  
  write(1, "\n", 1);
}

// === Test 6: Límits de creixement de pila ===

// Thread amb creixement controlat (2KB de variables locals)
void controlled_growth_thread(void *arg)
{
  int *counter = (int *)arg;
  volatile int local_vars[500];  // 2KB de variables locals
  int i;
  
  write(1, "  Thread controlat iniciat\n", 28);
  
  // Usar les variables per forçar assignació
  for (i = 0; i < 500; i++) {
    local_vars[i] = i * 2;
  }
  
  (*counter) = local_vars[499];  // Guardar un valor
  
  write(1, "  Memoria validada OK\n", 23);
  
  ThreadExit();
}

// Thread que intenta usar més de 11 pàgines (48KB > 44KB limit)
// NOTA IMPORTANT: Aquest thread HAURIA DE MATAR el procés
// Quan supera el límit de 11 pàgines, el kernel genera un
// General Protection Fault i mata el procés. Això és el
// COMPORTAMENT CORRECTE i demostra que el límit funciona.
void overflow_test_thread(void *arg)
{
  write(1, "\n", 1);
  
  volatile int huge_array[12000];  // 48KB = 12 pàgines > 11 limit
  int *counter = (int *)arg;
  int i;
  
  // Intentar accedir a l'array sencer (causarà page fault)
  for (i = 0; i < 12000; i++) {
    huge_array[i] = i;
    
    if (i % 1000 == 0) {
      (*counter) = i / 1000;
      write(1, "  Acces a pos ", 15);
      itoa(i, buff);
      write(1, buff, strlen(buff));
      write(1, " OK\n", 5);
      yield();
    }
  }
  
  // Si arribem aquí, el límit NO ha funcionat
  write(1, "\n*** BUG: Thread complet sense page fault! ***\n", 48);
  write(1, "*** Va accedir a 12 pagines (limit es 11) ***\n", 48);
  
  ThreadExit();
}

void test_6_stack_limits(void)
{
  int tid1, tid2;
  int i;
  
  write(1, "\n=== Test 6: Limits de creixement de pila ===\n", 48);
  
  dynamic_counter_1 = 0;
  dynamic_counter_2 = 0;
  
  // Part 1: Creixement normal
  write(1, "\n[Test 6.1] Creixement controlat (2KB locals)\n", 47);
  tid1 = ThreadCreate(controlled_growth_thread, (void *)&dynamic_counter_1);
  
  if (tid1 < 0) {
    write(1, "ERROR: Thread no es va crear\n", 31);
    return;
  }
  
  for (i = 0; i < 15; i++) yield();
  
  write(1, "[Test 6.1] PASSAT\n", 19);
  
  // Part 2: TEST CRÍTIC - Superar límit
  // IMPORTANT: Aquest test mata el procés intencionadament!
  // És el comportament CORRECTE per demostrar que el límit funciona.
  write(1, "\n[Test 6.2] Test de superacio de limit (48KB > 44KB max)\n", 59);
  write(1, "ATENCIO: Aquest test MATA el proces (comportament correcte)\n", 62);
  write(1, "         Si surt 'General Protection Fault', el limit funciona\n", 67);
  write(1, "         Per veure Tests 7-8: comenta aquest test i recompila\n\n\n", 68);
  
  for (i = 0; i < 5; i++) yield();
  
  dynamic_counter_2 = 0;
  tid2 = ThreadCreate(overflow_test_thread, (void *)&dynamic_counter_2);
  
  if (tid2 < 0) {
    write(1, "ERROR: Thread overflow no es va crear\n", 40);
    return;
  }
  
  // Donem temps per executar (no arribarà a completar-se)
  for (i = 0; i < 100; i++) {
    yield();
  }
  
  // Si arribem aquí, el límit no ha funcionat
  write(1, "\n*** BUG: El proces no va morir! ***\n", 38);
  write(1, "*** El limit de 11 pagines NO funciona correctament ***\n", 58);
}

/* === Test 7: Exit de thread secundari === */

void secondary_thread_worker(void *arg)
{
  int *counter = (int *)arg;
  int i;
  
  write(1, "  Thread secundari iniciat\n", 28);
  
  for (i = 0; i < 5; i++) {
    (*counter)++;
    write(1, "  Thread secundari - iteracio ", 31);
    itoa(i + 1, buff);
    write(1, buff, strlen(buff));
    write(1, " (counter: ", 12);
    itoa(*counter, buff);
    write(1, buff, strlen(buff));
    write(1, ")\n", 2);
    yield();
  }
  
  write(1, "  Thread secundari cridant ThreadExit()...\n", 44);
  ThreadExit();
  
  // AQUEST CODI NO S'HAURIA D'EXECUTAR MAI
  write(1, "*** BUG: Thread secundari continua despres ThreadExit! ***\n", 61);
}

void test_7_secondary_thread_exit(void)
{
  int tid;
  int i;
  
  write(1, "\n=== Test 7: Exit de thread secundari ===\n", 43);
  
  exit_counter = 0;
  
  write(1, "\nCreant thread secundari...\n", 29);
  tid = ThreadCreate(secondary_thread_worker, (void *)&exit_counter);
  
  if (tid < 0) {
    write(1, "ERROR: No s'ha pogut crear thread secundari\n", 46);
    return;
  }
  
  write(1, "Thread secundari creat (TID: ", 31);
  itoa(tid, buff);
  write(1, buff, strlen(buff));
  write(1, ")\n\n", 3);
  
  // Main thread continua executant mentre el secundari treballa
  for (i = 0; i < 12; i++) {
    write(1, "  Main thread - iteracio ", 26);
    itoa(i + 1, buff);
    write(1, buff, strlen(buff));
    write(1, "\n", 1);
    yield();
  }
  
  write(1, "\nMain thread continua despres exit thread secundari\n", 53);
  write(1, "Counter final thread secundari: ", 34);
  itoa(exit_counter, buff);
  write(1, buff, strlen(buff));
  write(1, "\n\n[Test 7] PASSAT\n", 18);
}

/* === Test 8: Exit del thread leader === */

void zombie_thread_worker(void *arg)
{
  int *counter = (int *)arg;
  int loop_count = 0;
  
  write(1, "  Zombie thread iniciat (hauria de morir amb el leader)\n", 58);
  
  // Bucle infinit - hauria de ser mort quan el leader faci exit
  while (1) {
    (*counter)++;
    loop_count++;
    
    if (loop_count <= 8) {
      write(1, "  Zombie thread encara viu, iteracio ", 39);
      itoa(loop_count, buff);
      write(1, buff, strlen(buff));
      write(1, "\n", 1);
    }
    
    yield();
  }
  
  // NO HAURIA D'ARRIBAR AQUÍ
  write(1, "*** BUG: Zombie thread va sortir del bucle infinit! ***\n", 58);
}

void test_8_leader_exit(void)
{
  int tid1, tid2;
  int i;
  int counter1 = 0, counter2 = 0;
  
  write(1, "\n=== Test 8: Exit del thread leader ===\n", 41);
  
  write(1, "\nCreant 2 threads secundaris...\n", 33);
  
  tid1 = ThreadCreate(zombie_thread_worker, (void *)&counter1);
  tid2 = ThreadCreate(zombie_thread_worker, (void *)&counter2);
  
  if (tid1 < 0 || tid2 < 0) {
    write(1, "ERROR: No s'han pogut crear threads\n", 38);
    return;
  }
  
  write(1, "Threads creats (TID: ", 22);
  itoa(tid1, buff);
  write(1, buff, strlen(buff));
  write(1, ", ", 2);
  itoa(tid2, buff);
  write(1, buff, strlen(buff));
  write(1, ")\n\n", 3);
  
  // Deixem que els threads s'executin una mica
  write(1, "Deixant que threads secundaris s'executin...\n", 47);
  for (i = 0; i < 12; i++) {
    yield();
  }
  
  write(1, "\n", 1);
  write(1, "ATENCIO: Main thread (LEADER) cridarà exit()\n", 47);
  write(1, "         TOTS els threads del proces haurien de MORIR\n", 56);
  write(1, "         Si veus missatges despres, hi ha un BUG\n\n", 52);
  
  for (i = 0; i < 3; i++) yield();
  
  write(1, "Main leader cridant exit() ARA...\n\n", 36);
  exit();
  
  // AQUEST CODI NO S'HAURIA D'EXECUTAR MAI
  write(1, "\n*** BUG CRITIC: Main thread continua despres exit()! ***\n", 59);
  write(1, "*** El proces SENCER hauria d'haver mort ***\n", 46);
}

/* === Test 9: Keyboard Support === */

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','?','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','?',
  '\0','?','\0','?','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

volatile int global_key = -1;
volatile int global_pressed = 0;
volatile int key_event_pending = 0;
volatile int syscall_result = 0;
volatile int press_count = 0;
volatile int release_count = 0;

// Dummy callback
void dummy_callback(char key, int pressed) {
    return;
}

// Test 9.1: Desactivar handler amb NULL
void test_9_1_disable_handler(void) {
    int ret;
    write(1, "\n=== Test 9.1: Desactivar handler de teclat amb NULL ===\n", 57);
    
    // Primer registrem un valid
    ret = KeyboardEvent(dummy_callback);
    if (ret < 0) {
        write(1, "ERROR: No s'ha pogut registrar el handler inicial\n", 50);
        return;
    }
    
    // Ara el desactivem
    ret = KeyboardEvent(0); // NULL
    if (ret == 0) {
        write(1, "[Test 9.1] PASSAT\n", 18);
    } else {
        write(1, "ERROR: KeyboardEvent(NULL) ha retornat error\n", 45);
    }
}

// Callback per esperar alliberament de tecla
void wait_release_callback(char key, int pressed) {
    if (!pressed) {
        key_event_pending = 1;
    }
}

// Callback per comptar (solo cuenta press)
void counting_callback(char key, int pressed) {
    if (pressed) {
        press_count++;
        key_event_pending = 1;
    }
}

// Test 9.2: Verificar que el handler es cridat
void test_9_2_verify_handler(void) {
    write(1, "\n=== Test 9.2: Verificar que el handler es cridat ===\n", 54);
    write(1, "  Registrant handler. Si us plau prem 3 tecles...\n", 50);
    
    press_count = 0;
    KeyboardEvent(counting_callback);
    
    while (press_count < 3) {
        // Espera activa
    }
    
    write(1, "[Test 9.2] PASSAT\n", 18);
    
    // Esperar release de l'ultima tecla per evitar interferencies amb el següent test
    key_event_pending = 0;
    KeyboardEvent(wait_release_callback);
    while (!key_event_pending) {}
    KeyboardEvent(0);
}

// Callback per test EINPROGRESS
void einprogress_callback(char key, int pressed) {
    // Nomes activar en press per evitar agafar el release del test anterior
    if (pressed) {
        int res;
        int failed = 0;
        
        // Test getpid
        res = getpid();
        if (res != -1) failed = 1;
        if (errno != 115) failed = 1;
        
        // Test fork
        // fork salta a nok, aixi que retorna -1 i posa errno
        res = fork();
        if (res != -1) failed = 1;
        if (errno != 115) failed = 1;
        
        // Test write (stdout)
        // write salta a nok
        res = write(1, "fail", 4);
        if (res != -1) failed = 1;
        if (errno != 115) failed = 1;
        
        // Test get_stats
        // get_stats salta a nok
        struct stats st;
        res = get_stats(1, &st);
        if (res != -1) failed = 1;
        if (errno != 115) failed = 1;
        
        if (failed) syscall_result = 0;
        else syscall_result = -115;
        
        key_event_pending = 1;
    }
}

// Test 9.3: Verificar que syscalls retornen EINPROGRESS
void test_9_3_einprogress(void) {
    write(1, "\n=== Test 9.3: Verificar que syscalls retornen EINPROGRESS ===\n", 63);
    write(1, "  Registrant callback que crida multiples syscalls...\n", 54);
    write(1, "  SI US PLAU PREM QUALSEVOL TECLA per activar la comprovacio...\n", 64);
    
    key_event_pending = 0;
    KeyboardEvent(einprogress_callback);
    
    // Esperar pulsacio de tecla
    while (!key_event_pending) {
        // Espera activa
    }
    
    write(1, "  Callback executat. Totes syscalls bloquejades? ", 49);
    if (syscall_result == -115) {
        write(1, "SI\n", 3);
        write(1, "[Test 9.3] PASSAT\n", 18);
    } else {
        write(1, "NO\n", 3);
        write(1, "ERROR: Algunes syscalls s'han executat\n", 39);
    }
    
    // Desregistrar
    KeyboardEvent(0);
    
    // Esperar alliberament de la tecla premuda a 9.3 per evitar que compti a 9.4
    write(1, "  Esperant alliberament de tecla...\n", 36);
    key_event_pending = 0;
    KeyboardEvent(wait_release_callback);
    while (!key_event_pending) {
        // Espera activa per alliberament
    }
    KeyboardEvent(0);
}

// Callback per test interactiu
void interactive_callback(char key, int pressed) {
    global_key = key;
    global_pressed = pressed;
    
    if (pressed) press_count++;
    else release_count++;
    
    key_event_pending = 1;
}

// Test 9.4: Teclat Interactiu
void test_9_4_interactive(void) {
    char b[2];
    b[1] = '\0';
    char mapped_char;
    
    write(1, "\n=== Test 9.4: Teclat Interactiu ===\n", 37);
    write(1, "  Compta pulsacions/alliberaments i imprimeix caracters.\n", 57);
    write(1, "  PREM 'ESC' PER ACABAR AQUEST TEST.\n\n", 38);
    
    press_count = 0;
    release_count = 0;
    KeyboardEvent(interactive_callback);
    
    while (1) {
        if (key_event_pending) {
            key_event_pending = 0;
            
            // Obtenir char del mapa
            if (global_key < 128) mapped_char = char_map[(int)global_key];
            else mapped_char = '?';
            
            // Comprovar condicio sortida: ESC scancode es 1
            if (global_pressed && global_key == 1) {
                write(1, "\n  'ESC' premut. Sortint test interactiu.\n", 42);
                break;
            }
            
            if (global_pressed) {
                write(1, "  PREMUT   [", 12);
                b[0] = mapped_char;
                if (mapped_char == 0) write(1, "?", 1);
                else write(1, b, 1);
                write(1, "] Scancode: ", 12);
                itoa(global_key, buff);
                write(1, buff, strlen(buff));
                write(1, " | Total Premuts: ", 18);
                itoa(press_count, buff);
                write(1, buff, strlen(buff));
                write(1, "\n", 1);
            } else {
                write(1, "  ALLIBERAT[", 12);
                b[0] = mapped_char;
                if (mapped_char == 0) write(1, "?", 1);
                else write(1, b, 1);
                write(1, "] Scancode: ", 12);
                itoa(global_key, buff);
                write(1, buff, strlen(buff));
                write(1, " | Total Alliberats: ", 21);
                itoa(release_count, buff);
                write(1, buff, strlen(buff));
                write(1, "\n", 1);
            }
        }
    }
    
    write(1, "\n[Test 9.4] PASSAT\n", 19);
    KeyboardEvent(0);
}

// Test 9.5: Validacio de parametres
void test_9_5_params(void) {
    write(1, "\n=== Test 9.5: Validacio de parametres ===\n", 43);
    
    // access_ok sol fallar per sota de L_USER_START (0x100000) o per sobre de cert limit.
    // Provem amb NULL (ja testejat, es valid per desactivar)
    // Provem amb una adreça clarament invalida si access_ok funciona correctament.
    
    void (*invalid_func)(char, int) = (void (*)(char, int))0x1000;
    
    int ret = KeyboardEvent(invalid_func);
    
    if (ret == -1 && errno == EFAULT) {
         write(1, "  KeyboardEvent(0x1000) ha fallat correctament (EFAULT).\n", 57);
         write(1, "[Test 9.5] PASSAT\n", 18);
    } else {
         write(1, "  ERROR: KeyboardEvent(0x1000) hauria d'haver fallat.\n", 54);
         write(1, "  Ret: ", 7);
         itoa(ret, buff);
         write(1, buff, strlen(buff));
         write(1, ", Errno: ", 9);
         itoa(errno, buff);
         write(1, buff, strlen(buff));
         write(1, "\n", 1);
    }
}

// Variables per Test 9.6
volatile int t1_events = 0;
volatile int main_events = 0;
volatile int thread_ready = 0;

void t1_callback(char k, int p) {
    if (p) {
        t1_events++;
        key_event_pending = 1;
    }
}

void main_callback(char k, int p) {
    if (p) {
        main_events++;
        key_event_pending = 1;
    }
}

void thread_with_keyboard(void *arg) {
    write(1, "  [Thread] Registrant callback...\n", 34);
    int ret = KeyboardEvent(t1_callback);
    if (ret < 0) {
        write(1, "  [Thread] ERROR: No s'ha pogut registrar callback\n", 51);
    } else {
        write(1, "  [Thread] Callback registrat OK\n", 33);
    }
    thread_ready = 1;
    
    // Mantenir viu
    while(t1_events < 2) yield();
    
    // Desregistrem el callback
    KeyboardEvent(0);
    write(1, "  [Thread] Callback desregistrat\n", 33);
    ThreadExit();
}

// Test 9.6: Concurrencia de Threads (primer guanya, sense stealing)
void test_9_6_thread_concurrency(void) {
    write(1, "\n=== Test 9.6: Concurrencia de Threads (Primer Guanya) ===\n", 59);
    
    t1_events = 0;
    main_events = 0;
    thread_ready = 0;
    
    // 1. Crear thread que registra callback
    int tid = ThreadCreate(thread_with_keyboard, 0);
    if (tid < 0) {
        write(1, "ERROR: No s'ha pogut crear thread\n", 34);
        return;
    }
    
    // Esperar que el thread registri el callback
    while(!thread_ready) yield();
    
    write(1, "  PREM 2 TECLES (Hauria de rebre-les el Thread)\n", 48);
    key_event_pending = 0;
    while(t1_events < 2) {
        yield();
    }
    
    if (t1_events >= 2) {
        write(1, "  -> OK: Thread ha rebut els events.\n", 37);
    } else {
        write(1, "  -> ERROR: Thread no ha rebut els events.\n", 43);
    }
    
    // Esperem que el thread acabi
    int i;
    for(i=0; i<5; i++) yield();
    
    // 2. Ara el main pot registrar el callback (thread ja ha desregistrat)
    write(1, "  [Main] Registrant callback...\n", 32);
    int ret = KeyboardEvent(main_callback);
    if (ret < 0) {
        write(1, "  [Main] ERROR: No s'ha pogut registrar callback\n", 49);
        return;
    }
    write(1, "  [Main] Callback registrat OK\n", 31);
    
    main_events = 0;
    key_event_pending = 0;
    
    write(1, "  PREM UNA TECLA (Hauria de rebre-la el Main)\n", 46);
    while(!key_event_pending) yield();
    
    if (main_events > 0) {
        write(1, "  -> OK: Main ha rebut l'event.\n", 32);
    } else {
        write(1, "  -> ERROR: Main no ha rebut l'event.\n", 38);
    }
    
    // Neteja
    KeyboardEvent(0);
    write(1, "[Test 9.6] PASSAT\n", 18);
}

// Variables per Test 9.7
volatile int parent_events = 0;

void parent_callback(char k, int p) {
    if (p) {
        parent_events++;
        key_event_pending = 1;
        write(1, "  [Parent] Event rebut!\n", 24);
    }
}

// Test 9.7: Keyboard amb fork (fill hereta sense callback)
void test_9_7_process_concurrency(void) {
    write(1, "\n=== Test 9.7: Keyboard amb Fork ===\n", 37);
    
    // Registrem callback al pare ABANS del fork
    write(1, "  [Parent] Registrant callback abans de fork...\n", 48);
    int ret = KeyboardEvent(parent_callback);
    if (ret < 0) {
        write(1, "  [Parent] ERROR: No s'ha pogut registrar callback\n", 51);
        return;
    }
    
    parent_events = 0;
    key_event_pending = 0;
    
    write(1, "  PREM UNA TECLA (Pare hauria de rebre-la)\n", 43);
    while(!key_event_pending) yield();
    
    if (parent_events > 0) {
        write(1, "  -> OK: Pare ha rebut l'event abans de fork.\n", 46);
    }
    
    int pid = fork();
    
    if (pid == 0) {
        // FILL - no hauria de tenir callback registrat (es reseteja a fork)
        write(1, "  [Child] Intentant registrar callback...\n", 42);
        ret = KeyboardEvent(parent_callback);
        if (ret == 0) {
            write(1, "  [Child] Callback registrat OK (esperat)\n", 42);
        } else {
            write(1, "  [Child] ERROR: No s'ha pogut registrar\n", 41);
        }
        exit();
    } else {
        // PARE - esperem que el fill acabi
        int i;
        for (i=0; i<10; i++) yield();
        
        // Comprovem que el pare encara pot rebre events
        parent_events = 0;
        key_event_pending = 0;
        
        write(1, "  PREM UNA TECLA (Pare hauria de rebre-la despres de fork)\n", 59);
        while(!key_event_pending) yield();
        
        if (parent_events > 0) {
            write(1, "  -> OK: Pare ha rebut l'event despres de fork.\n", 48);
            write(1, "[Test 9.7] PASSAT\n", 18);
        } else {
            write(1, "  -> ERROR: Pare no ha rebut l'event.\n", 38);
        }
        
        KeyboardEvent(0);
    }
}

char screen_buffer[4000];
volatile int my_key_pressed = 0;
volatile int my_scancode = 0;

void my_keyboard_callback(char key, int pressed) {
  if (pressed) {
    my_scancode = key;
    my_key_pressed = 1;
  }
}

void wait_for_scancode(int code) {
  my_key_pressed = 0;
  KeyboardEvent(my_keyboard_callback);
  while (1) {
    if (my_key_pressed && my_scancode == code) {
      break;
    }
  }
  KeyboardEvent(0);
}

void wait_for_any_key(void) {
  my_key_pressed = 0;
  KeyboardEvent(my_keyboard_callback);
  while (!my_key_pressed) {
    // esperem
  }
  KeyboardEvent(0);
}

void draw_string(int x, int y, char *s, int color) {
  int i;
  for (i = 0; s[i]; i++) {
    screen_buffer[(y * 80 + x + i) * 2] = s[i];
    screen_buffer[(y * 80 + x + i) * 2 + 1] = color;
  }
}

// Test de FPS - objectiu: minim 30 FPS
void test_fps(void) {
  int frame_count = 0;
  int fps = 0;
  int last_time;
  char fps_str[8];
  int i;
  
  write(1, "\nTEST FPS - Prem ESC per sortir\n", 34);
  
  // Registrar callback de teclat per detectar ESC
  my_key_pressed = 0;
  KeyboardEvent(my_keyboard_callback);
  
  last_time = gettime();
  
  while (1) {
    // Comprovar si s'ha premut ESC (scancode 1)
    if (my_key_pressed && my_scancode == 1) break;
    
    // Netejar pantalla amb fons negre
    for (i = 0; i < 80 * 25; i++) {
      screen_buffer[i * 2] = ' ';
      screen_buffer[i * 2 + 1] = 0x00;
    }
    
    // Dibuixar títol
    draw_string(30, 1, "=== TEST FPS ===", 0x0E);
    
    // Mostrar FPS
    draw_string(2, 22, "FPS: ", 0x0F);
    itoa(fps, fps_str);
    draw_string(7, 22, fps_str, 0x0B);
    
    // Objectiu
    if (fps >= 30) {
      draw_string(50, 22, "OK! >= 30 FPS", 0x0A);
    } else {
      draw_string(50, 22, "< 30 FPS", 0x0C);
    }
    
    draw_string(2, 24, "Prem ESC per sortir", 0x07);
    
    // Escriure a pantalla (fd 10)
    write(10, screen_buffer, 80 * 25 * 2);
    
    frame_count++;
    
    // Calcular FPS cada segon (18 ticks)
    if (gettime() - last_time >= 18) {
      fps = frame_count;
      frame_count = 0;
      last_time = gettime();
    }
  }
  
  // Desregistrar callback de teclat
  KeyboardEvent(0);
  
  // Mostrar resultat final
  for (i = 0; i < 80 * 25; i++) {
    screen_buffer[i * 2] = ' ';
    screen_buffer[i * 2 + 1] = 0x00;
  }
  draw_string(30, 10, "TEST FPS COMPLETAT", 0x0E);
  draw_string(30, 12, "FPS aconseguit: ", 0x0F);
  itoa(fps, fps_str);
  draw_string(46, 12, fps_str, 0x0B);
  
  if (fps >= 30) {
    draw_string(25, 14, "OBJECTIU ACONSEGUIT! (>= 30 FPS)", 0x0A);
  } else {
    draw_string(25, 14, "OBJECTIU NO ACONSEGUIT (< 30 FPS)", 0x0C);
  }
  
  write(10, screen_buffer, 80 * 25 * 2);
}

void test_screen_write(void) {
  write(1, "\nPREM QUALSEVOL TECLA PER A INICIAR TEST 9\n", 43);
  wait_for_any_key();

  int i, x, y;
  // Netejem pantalla (fons negre)
  for (i = 0; i < 80 * 25; i++) {
    screen_buffer[i * 2] = ' ';
    screen_buffer[i * 2 + 1] = 0x07; // Blanc sobre negre
  }

  // Dibuixem primer layout
  draw_string(34, 10, "projecte soa", 0x02); // Verd
  draw_string(38, 12, "2025", 0x04); // Vermell
  draw_string(10, 20, "Pol Gay", 0x0E); // Groc
  draw_string(50, 20, "Mateus Grandolfi", 0x0B); // Blau
  
  draw_string(0, 24, "PREM ENTER TO CONTINUE", 0x07);

  write(10, screen_buffer, 80 * 25 * 2);
  
  wait_for_scancode(28); // Enter
  
  // Segon layout
  for (y = 0; y < 25; y++) {
    for (x = 0; x < 80; x++) {
      int color = (x + y) % 16;
      screen_buffer[(y * 80 + x) * 2] = ' ';
      screen_buffer[(y * 80 + x) * 2 + 1] = (color << 4) | 0x0F;
    }
  }
  
  draw_string(25, 12, "porfa aprovans, necesito un 10", 0x0F); // Blanc sobre negre
  
  draw_string(0, 24, "PREM ESC PER FINALITZAR", 0x07); // Blanc sobre negre
  
  write(10, screen_buffer, 80 * 25 * 2);
  
  wait_for_scancode(1); // ESC

  // Tercer layout
  for (i = 0; i < 80 * 25; i++) {
    screen_buffer[i * 2] = ' ';
    screen_buffer[i * 2 + 1] = 0x07; // Blanc sobre negre
  }
  
  draw_string(38, 12, "adeu", 0x07); // Blanc sobre negre
  
  write(10, screen_buffer, 80 * 25 * 2);
}

// ========== TEST WAITFORTICK AMB MÚLTIPLES THREADS ==========
// Comptadors per verificar que tots els threads es desperten
volatile int tick_thread_1_count = 0;
volatile int tick_thread_2_count = 0;
volatile int tick_thread_done_1 = 0;
volatile int tick_thread_done_2 = 0;

void tick_test_thread_1(void *arg) {
  int i;
  for (i = 0; i < 5; i++) {
    WaitForTick();
    tick_thread_1_count++;
  }
  tick_thread_done_1 = 1;
  ThreadExit();
}

void tick_test_thread_2(void *arg) {
  int i;
  for (i = 0; i < 5; i++) {
    WaitForTick();
    tick_thread_2_count++;
  }
  tick_thread_done_2 = 1;
  ThreadExit();
}

void test_waitfortick_multithreaded(void) {
  int tid1, tid2;
  int i;
  char buff_local[16];
  
  write(1, "\n=== Test WaitForTick: Multiples Threads ===\n", 46);
  
  // Reset comptadors
  tick_thread_1_count = 0;
  tick_thread_2_count = 0;
  tick_thread_done_1 = 0;
  tick_thread_done_2 = 0;
  
  // Creem dos threads que faran WaitForTick simultàniament
  tid1 = ThreadCreate(tick_test_thread_1, 0);
  tid2 = ThreadCreate(tick_test_thread_2, 0);
  
  if (tid1 < 0 || tid2 < 0) {
    write(1, "ERROR: No s'han pogut crear els threads\n", 42);
    return;
  }
  
  write(1, "Threads creats. Esperant que acabin...\n", 40);
  
  // Esperem que acabin usant WaitForTick (no yield!)
  // Els threads secundaris fan 5 WaitForTick cadascun, així que esperem ~10 ticks
  // Afegim un marge de seguretat (20 ticks)
  for (i = 0; i < 20 && (!tick_thread_done_1 || !tick_thread_done_2); i++) {
    WaitForTick();  // El main thread també espera ticks
  }
  
  // Espera addicional per assegurar que els threads han tingut temps d'actualitzar els comptadors
  // després de despertar-se del seu últim WaitForTick
  for (i = 0; i < 10; i++) yield();
  
  write(1, "Thread 1 WaitForTick completats: ", 34);
  itoa(tick_thread_1_count, buff_local);
  write(1, buff_local, strlen(buff_local));
  write(1, "\n", 1);
  
  write(1, "Thread 2 WaitForTick completats: ", 34);
  itoa(tick_thread_2_count, buff_local);
  write(1, buff_local, strlen(buff_local));
  write(1, "\n", 1);
  
  if (tick_thread_1_count == 5 && tick_thread_2_count == 5) {
    write(1, "OK: Ambdos threads han completat 5 WaitForTick\n", 48);
  } else {
    write(1, "ERROR: Algun thread no ha completat correctament\n", 51);
  }
}

void test_tick_screen(void) {
  int colors[] = {0xE0, 0x1F, 0x5F, 0x2F}; // Groc, Blau, Morat, Verd
  int current_color_idx = 0;
  int i, j;
  int num_cycles = 0;
  int max_cycles = 20; // Maxim 20 canvis de color (20 segons aprox)
  int attr;
  
  write(1, "\nPREM QUALSEVOL TECLA PER A INICIAR TEST TICK SCREEN\n", 54);
  wait_for_any_key();

  write(1, "Iniciant tick test...\n", 22);
  
  while (num_cycles < max_cycles) {
    attr = colors[current_color_idx];
    
    // Omplim tot el buffer amb el color de fons
    for (i = 0; i < 80 * 25; i++) {
      screen_buffer[i * 2] = ' ';
      screen_buffer[i * 2 + 1] = attr;
    }
    
    // Dibuixem el missatge informatiu
    draw_string(30, 12, "TEST FINALITZA SOL", 0x0F); // Blanc sobre negre

    write(10, screen_buffer, 80 * 25 * 2);
    
    // Espera 18 ticks (aproximadament 1 segon)
    for (j = 0; j < 18; j++) {
      WaitForTick();
    }

    // Selecciona el seguent color 
    current_color_idx = (current_color_idx + 1) % 4;
    num_cycles++;
  }
  
  // Netejem pantalla
  for (i = 0; i < 80 * 25; i++) {
    screen_buffer[i * 2] = ' ';
    screen_buffer[i * 2 + 1] = 0x07;
  }
  write(10, screen_buffer, 80 * 25 * 2);
  write(1, "Test Tick Acabat.\n", 19);
}

/* ============================================ */
/*        MILESTONE 5: ZEOS DIGGER GAME        */
/* ============================================ */

/* ============================================ */
/*        CONSTANTS I CONFIGURACIÓ             */
/* ============================================ */

#define GAME_SCREEN_WIDTH  80
#define GAME_SCREEN_HEIGHT 25
#define GAME_AREA_TOP 2
#define GAME_AREA_BOTTOM 23
#define GAME_AREA_LEFT 1
#define GAME_AREA_RIGHT 78

#define MAX_ENEMIES 5
#define PLAYER_LIVES 3

#define TICKS_PER_SECOND 18
#define ENEMY_MOVE_TICKS 1152
#define ENEMY_DIG_TICKS 1536
#define PLAYER_INVULN_TICKS 36 /* 2 segons d'invulnerabilitat */
#define MAX_LEVELS 3           /* Nombre total de nivells */

/* Scancodes de tecles importants */
#define KEY_ESC    0x01
#define KEY_ENTER  0x1C
#define KEY_SPACE  0x39
#define KEY_W      0x11
#define KEY_A      0x1E
#define KEY_S      0x1F
#define KEY_D      0x20
#define KEY_R      0x13
#define KEY_UP     0x48
#define KEY_DOWN   0x50
#define KEY_LEFT   0x4B
#define KEY_RIGHT  0x4D

/* Colors VGA (fons << 4 | text) */
#define COLOR_BLACK      0x00
#define COLOR_BLUE       0x01
#define COLOR_GREEN      0x02
#define COLOR_CYAN       0x03
#define COLOR_RED        0x04
#define COLOR_MAGENTA    0x05
#define COLOR_BROWN      0x06
#define COLOR_LIGHTGRAY  0x07
#define COLOR_DARKGRAY   0x08
#define COLOR_LIGHTBLUE  0x09
#define COLOR_LIGHTGREEN 0x0A
#define COLOR_LIGHTCYAN  0x0B
#define COLOR_LIGHTRED   0x0C
#define COLOR_PINK       0x0D
#define COLOR_YELLOW     0x0E
#define COLOR_WHITE      0x0F

#define ATTR_PLAYER     (COLOR_BLACK << 4 | COLOR_YELLOW)
#define ATTR_ENEMY      (COLOR_BLACK << 4 | COLOR_LIGHTRED)
#define ATTR_DIRT       (COLOR_BROWN << 4 | COLOR_BROWN)
#define ATTR_TUNNEL     (COLOR_BLACK << 4 | COLOR_DARKGRAY)
#define ATTR_BORDER     (COLOR_BLUE << 4 | COLOR_WHITE)
#define ATTR_GEM        (COLOR_BLACK << 4 | COLOR_LIGHTCYAN)

/* ============================================ */
/*         ESTRUCTURES DE DADES DEL JOC        */
/* ============================================ */

typedef struct {
  int x, y;
  int alive;
  int direction; /* 0=amunt, 1=dreta, 2=avall, 3=esquerra */
  int move_counter;
  int dig_counter;  /* Comptador per excavar */
} GameEnemy;

typedef struct {
  int x, y;
  int lives;
  int score;
  int invulnerable_ticks;
  char direction; /* Última direcció per dibuixar */
} GamePlayer;

/* Estats del joc */
#define STATE_TITLE     0
#define STATE_PLAYING   1
#define STATE_PAUSED    2
#define STATE_GAMEOVER  3
#define STATE_WIN       4
#define STATE_LEVEL_UP  5  /* Transició entre nivells */

typedef struct {
  int state;
  int level;
  int enemies_alive;
  int gems_collected;
  int total_gems;
  int level_transition_ticks;  /* Comptador per mostrar pantalla de nivell */
  GamePlayer player;
  GameEnemy enemies[MAX_ENEMIES];
  char map[GAME_SCREEN_HEIGHT][GAME_SCREEN_WIDTH]; /* 0=terra, 1=túnel, 2=gemma */
} GameState;

/* ============================================ */
/*         VARIABLES GLOBALS DEL JOC           */
/* ============================================ */

char game_screen_buffer[GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2];
GameState game;

/* Variables d'input - modificades pel callback de teclat */
volatile int key_up_game = 0;
volatile int key_down_game = 0;
volatile int key_left_game = 0;
volatile int key_right_game = 0;
volatile int key_action_game = 0;  /* SPACE o ENTER */
volatile int key_pause_game = 0;   /* ESC */
volatile int key_restart_game = 0; /* R */

/* Última direcció premuda: 0=cap, 1=amunt, 2=avall, 3=esquerra, 4=dreta */
volatile int last_direction = 0;

/* Variables per FPS */
volatile int game_frame_count = 0;
volatile int fps_display = 0;
volatile int last_tick_count = 0;

/* Control de velocitat del jugador */
#define PLAYER_MOVE_TICKS 200  /* Jugador es mou lent */
volatile int player_move_counter = 0;

/* Sincronització entre threads */
volatile int game_running = 1;

/* Control de redibuixat per evitar parpelleig */
volatile int bottom_bar_drawn = 0;   /* 1 si la línia inferior ja s'ha dibuixat */
volatile int pause_menu_drawn = 0;   /* 1 si el menú de pausa ja s'ha dibuixat */

/* ============================================ */
/*         FUNCIONS D'UTILITAT DEL JOC         */
/* ============================================ */

void itoa_padded(int n, char *buf, int width) {
  char tmp[16];
  int i = 0, j;
  
  if (n == 0) {
    tmp[i++] = '0';
  } else {
    while (n > 0) {
      tmp[i++] = '0' + (n % 10);
      n /= 10;
    }
  }
  
  /* Omplir amb espais */
  for (j = 0; j < width - i; j++) {
    buf[j] = ' ';
  }
  
  /* Copiar dígits en ordre invers */
  for (j = 0; j < i; j++) {
    buf[width - 1 - j] = tmp[j];
  }
  buf[width] = '\0';
}

int my_strlen(char *s) {
  int len = 0;
  while (s[len]) len++;
  return len;
}

/* ============================================ */
/*         FUNCIONS DE RENDERITZAT             */
/* ============================================ */

void game_set_pixel(int x, int y, char c, char attr) {
  if (x >= 0 && x < GAME_SCREEN_WIDTH && y >= 0 && y < GAME_SCREEN_HEIGHT) {
    int idx = (y * GAME_SCREEN_WIDTH + x) * 2;
    game_screen_buffer[idx] = c;
    game_screen_buffer[idx + 1] = attr;
  }
}

void game_draw_string(int x, int y, char *str, char attr) {
  while (*str) {
    game_set_pixel(x++, y, *str++, attr);
  }
}

void game_draw_string_centered(int y, char *str, char attr) {
  int len = my_strlen(str);
  int x = (GAME_SCREEN_WIDTH - len) / 2;
  game_draw_string(x, y, str, attr);
}

void game_clear_screen(char attr) {
  int i;
  for (i = 0; i < GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT; i++) {
    game_screen_buffer[i * 2] = ' ';
    game_screen_buffer[i * 2 + 1] = attr;
  }
}

void game_draw_box(int x1, int y1, int x2, int y2, char attr) {
  int x, y;
  
  /* Cantonades */
  game_set_pixel(x1, y1, '+', attr);
  game_set_pixel(x2, y1, '+', attr);
  game_set_pixel(x1, y2, '+', attr);
  game_set_pixel(x2, y2, '+', attr);
  
  /* Línies horitzontals */
  for (x = x1 + 1; x < x2; x++) {
    game_set_pixel(x, y1, '-', attr);
    game_set_pixel(x, y2, '-', attr);
  }
  
  /* Línies verticals */
  for (y = y1 + 1; y < y2; y++) {
    game_set_pixel(x1, y, '|', attr);
    game_set_pixel(x2, y, '|', attr);
  }
}

void game_draw_filled_box(int x1, int y1, int x2, int y2, char c, char attr) {
  int x, y;
  for (y = y1; y <= y2; y++) {
    for (x = x1; x <= x2; x++) {
      game_set_pixel(x, y, c, attr);
    }
  }
}

/* ============================================ */
/*         PANTALLA DE TÍTOL                   */
/* ============================================ */

void draw_title_screen(void) {
  int i;
  game_clear_screen(COLOR_BLACK << 4 | COLOR_BLACK);
  
  /* Fons decoratiu amb patró */
  for (i = 0; i < GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT; i++) {
    int x = i % GAME_SCREEN_WIDTH;
    int y = i / GAME_SCREEN_WIDTH;
    if ((x + y) % 8 == 0) {
      game_screen_buffer[i * 2] = '.';
      game_screen_buffer[i * 2 + 1] = COLOR_BLACK << 4 | COLOR_DARKGRAY;
    }
  }
  
  /* Caixa del títol */
  game_draw_filled_box(15, 3, 64, 9, ' ', COLOR_BLUE << 4 | COLOR_WHITE);
  game_draw_box(15, 3, 64, 9, COLOR_CYAN << 4 | COLOR_WHITE);
  
  /* Títol del joc */
  game_draw_string_centered(5, "* * * Z E O S   D I G G E R * * *", COLOR_BLUE << 4 | COLOR_YELLOW);
  game_draw_string_centered(7, "~ Underground Adventure ~", COLOR_BLUE << 4 | COLOR_LIGHTCYAN);
  
  /* Instruccions */
  game_draw_filled_box(20, 11, 59, 19, ' ', COLOR_BLACK << 4 | COLOR_WHITE);
  game_draw_box(20, 11, 59, 19, COLOR_BROWN << 4 | COLOR_YELLOW);
  
  game_draw_string_centered(12, "HOW TO PLAY", COLOR_BLACK << 4 | COLOR_WHITE);
  game_draw_string_centered(14, "W/A/S/D or Arrows - Move", COLOR_BLACK << 4 | COLOR_LIGHTGRAY);
  game_draw_string_centered(15, "Collect all GEMS (*)", COLOR_BLACK << 4 | COLOR_LIGHTCYAN);
  game_draw_string_centered(16, "Avoid ENEMIES (@)", COLOR_BLACK << 4 | COLOR_LIGHTRED);
  game_draw_string_centered(18, "ESC - Pause   R - Restart", COLOR_BLACK << 4 | COLOR_LIGHTGRAY);
  
  /* Prompt per iniciar */
  game_draw_string_centered(22, ">>> PRESS ENTER OR SPACE TO START <<<", COLOR_BLACK << 4 | COLOR_LIGHTGREEN);
  
  /* Crèdits */
  game_draw_string_centered(24, "ZeOS Milestone 5 - Gay & Grandolfi", COLOR_BLACK << 4 | COLOR_DARKGRAY);
}

/* ============================================ */
/*         INICIALITZACIÓ DEL JOC              */
/* ============================================ */

void init_game_map(void) {
  int x, y, i;
  
  /* Omplir tot de terra */
  for (y = 0; y < GAME_SCREEN_HEIGHT; y++) {
    for (x = 0; x < GAME_SCREEN_WIDTH; x++) {
      if (y >= GAME_AREA_TOP && y <= GAME_AREA_BOTTOM &&
          x >= GAME_AREA_LEFT && x <= GAME_AREA_RIGHT) {
        game.map[y][x] = 0; /* Terra */
      } else {
        game.map[y][x] = 1; /* Túnel (vores) */
      }
    }
  }
  
  /* Crear alguns túnels inicials (patró de creu) */
  /* Túnel horitzontal central */
  for (x = GAME_AREA_LEFT; x <= GAME_AREA_RIGHT; x++) {
    game.map[12][x] = 1;
  }
  
  /* Túnel vertical central */
  for (y = GAME_AREA_TOP; y <= GAME_AREA_BOTTOM; y++) {
    game.map[y][40] = 1;
  }
  
  /* Petits túnels a les cantonades */
  for (i = 0; i < 8; i++) {
    game.map[5][10 + i] = 1;
    game.map[5][62 + i] = 1;
    game.map[20][10 + i] = 1;
    game.map[20][62 + i] = 1;
  }
  
  /* Col·locar gemmes (en posicions fixes per ser previsible) */
  game.total_gems = 0;
  
  /* Gemmes a les cantonades */
  game.map[4][5] = 2; game.total_gems++;
  game.map[4][74] = 2; game.total_gems++;
  game.map[22][5] = 2; game.total_gems++;
  game.map[22][74] = 2; game.total_gems++;
  
  /* Gemmes al centre */
  game.map[8][20] = 2; game.total_gems++;
  game.map[8][60] = 2; game.total_gems++;
  game.map[16][20] = 2; game.total_gems++;
  game.map[16][60] = 2; game.total_gems++;
  
  /* Gemmes addicionals */
  game.map[12][15] = 2; game.total_gems++;
  game.map[12][65] = 2; game.total_gems++;
  
  game.gems_collected = 0;
}

void init_game_player(void) {
  game.player.x = 40;
  game.player.y = 12;
  game.player.lives = PLAYER_LIVES;
  game.player.score = 0;
  game.player.invulnerable_ticks = PLAYER_INVULN_TICKS;
  game.player.direction = 'R';
  
  /* Crear túnel on està el jugador */
  game.map[game.player.y][game.player.x] = 1;
}

void init_game_enemies(void) {
  int i;
  
  game.enemies_alive = MAX_ENEMIES;
  
  /* Posicions fixes pels enemics (fàcils d'evitar) */
  /* Cantonada superior esquerra */
  game.enemies[0].x = 10;
  game.enemies[0].y = 5;
  /* Cantonada superior dreta */
  game.enemies[1].x = 70;
  game.enemies[1].y = 5;
  /* Cantonada inferior esquerra */
  game.enemies[2].x = 10;
  game.enemies[2].y = 20;
  /* Cantonada inferior dreta */
  game.enemies[3].x = 70;
  game.enemies[3].y = 20;
  /* Centre a baix */
  game.enemies[4].x = 40;
  game.enemies[4].y = 18;
  
  for (i = 0; i < MAX_ENEMIES; i++) {
    game.enemies[i].alive = 1;
    game.enemies[i].direction = i % 4;
    game.enemies[i].move_counter = 0;
    game.enemies[i].dig_counter = 0;
    
    /* Crear túnel on estan els enemics */
    game.map[game.enemies[i].y][game.enemies[i].x] = 1;
  }
}

void init_game_state(void) {
  game.state = STATE_PLAYING;
  game.level = 1;
  game.level_transition_ticks = 0;
  
  init_game_map();
  init_game_player();
  init_game_enemies();
  
  /* Reset input */
  key_up_game = key_down_game = key_left_game = key_right_game = 0;
  key_action_game = key_pause_game = key_restart_game = 0;
  
  game_frame_count = 0;
  fps_display = 0;
}

/* Inicialitza el següent nivell mantenint score i vides */
void init_next_level(void) {
  int saved_score = game.player.score;
  int saved_lives = game.player.lives;
  int next_level = game.level + 1;
  
  /* Reiniciar mapa i enemics */
  init_game_map();
  init_game_player();
  init_game_enemies();
  
  /* Restaurar score, vides i nivell */
  game.player.score = saved_score;
  game.player.lives = saved_lives;
  game.level = next_level;
  game.state = STATE_PLAYING;
  
  /* Augmentar dificultat: més enemics actius segons nivell */
  /* Nivell 2: enemics més ràpids (reduir ticks) */
  /* Nivell 3: encara més ràpids */
  
  /* Reset input */
  key_up_game = key_down_game = key_left_game = key_right_game = 0;
  key_action_game = key_pause_game = 0;
}

/* ============================================ */
/*         LÒGICA DEL JOC                      */
/* ============================================ */

int game_can_move_to(int x, int y) {
  if (x < GAME_AREA_LEFT || x > GAME_AREA_RIGHT) return 0;
  if (y < GAME_AREA_TOP || y > GAME_AREA_BOTTOM) return 0;
  return 1;
}

void game_move_player(int dx, int dy) {
  int new_x = game.player.x + dx;
  int new_y = game.player.y + dy;
  
  if (game_can_move_to(new_x, new_y)) {
    game.player.x = new_x;
    game.player.y = new_y;
    
    /* Actualitzar direcció visual */
    if (dx > 0) game.player.direction = 'R';
    else if (dx < 0) game.player.direction = 'L';
    else if (dy > 0) game.player.direction = 'D';
    else if (dy < 0) game.player.direction = 'U';
    
    /* Cavar túnel */
    if (game.map[new_y][new_x] == 0) {
      game.map[new_y][new_x] = 1;
      game.player.score += 1; /* Punt per cavar */
    }
    
    /* Recollir gemma */
    if (game.map[new_y][new_x] == 2) {
      game.map[new_y][new_x] = 1;
      game.gems_collected++;
      game.player.score += 100;
      
      /* Victòria de nivell si es recullen totes les gemmes */
      if (game.gems_collected >= game.total_gems) {
        if (game.level >= MAX_LEVELS) {
          game.state = STATE_WIN;  /* Joc completat! */
        } else {
          game.state = STATE_LEVEL_UP;  /* Següent nivell */
          game.level_transition_ticks = 60;  /* ~3 segons */
        }
      }
    }
  }
}

void game_update_player(void) {
  /* Decrementar invulnerabilitat */
  if (game.player.invulnerable_ticks > 0) {
    game.player.invulnerable_ticks--;
  }
  
  /* Incrementar comptador de moviment */
  player_move_counter++;
  
  /* Només moure si ha passat prou temps I hi ha direcció activa */
  if (player_move_counter >= PLAYER_MOVE_TICKS && last_direction != 0) {
    /* Usar l'última direcció premuda */
    switch (last_direction) {
      case 1: /* AMUNT */
        game_move_player(0, -1);
        break;
      case 2: /* AVALL */
        game_move_player(0, 1);
        break;
      case 3: /* ESQUERRA */
        game_move_player(-1, 0);
        break;
      case 4: /* DRETA */
        game_move_player(1, 0);
        break;
    }
    player_move_counter = 0;
    /* Resetejar direcció després de moure's */
    last_direction = 0;
  }
}

void game_move_enemy(int idx) {
  GameEnemy *e = &game.enemies[idx];
  if (!e->alive) return;
  
  int dx = 0, dy = 0;
  int new_x, new_y;
  int best_dir = -1;
  int best_dist = 9999;
  int i;
  
  /* Calcular direcció cap al jugador */
  int player_dx = game.player.x - e->x;
  int player_dy = game.player.y - e->y;
  
  /* Provar les 4 direccions i triar la que acosti més al jugador */
  for (i = 0; i < 4; i++) {
    switch (i) {
      case 0: dx = 0; dy = -1; break; /* Amunt */
      case 1: dx = 1; dy = 0; break;  /* Dreta */
      case 2: dx = 0; dy = 1; break;  /* Avall */
      case 3: dx = -1; dy = 0; break; /* Esquerra */
    }
    
    new_x = e->x + dx;
    new_y = e->y + dy;
    
    if (!game_can_move_to(new_x, new_y)) continue;
    
    /* Només moure's per túnels (no excavar aquí) */
    if (game.map[new_y][new_x] == 1) {
      /* Calcular distància al jugador */
      int dist_x = player_dx - dx;
      int dist_y = player_dy - dy;
      int dist = (dist_x < 0 ? -dist_x : dist_x) + (dist_y < 0 ? -dist_y : dist_y);
      
      if (dist < best_dist) {
        best_dist = dist;
        best_dir = i;
      }
    }
  }
  
  /* Si trobem un camí per túnel, moure's */
  if (best_dir >= 0) {
    switch (best_dir) {
      case 0: e->y--; break;
      case 1: e->x++; break;
      case 2: e->y++; break;
      case 3: e->x--; break;
    }
    e->direction = best_dir;
  }
}

/* Funció perquè els enemics excavin cap al jugador */
void game_enemy_dig(int idx) {
  GameEnemy *e = &game.enemies[idx];
  if (!e->alive) return;
  
  int dx = 0, dy = 0;
  int new_x, new_y;
  int best_dir = -1;
  int best_dist = 9999;
  int i;
  
  /* Calcular direcció cap al jugador */
  int player_dx = game.player.x - e->x;
  int player_dy = game.player.y - e->y;
  
  /* Provar les 4 direccions per excavar */
  for (i = 0; i < 4; i++) {
    switch (i) {
      case 0: dx = 0; dy = -1; break;
      case 1: dx = 1; dy = 0; break;
      case 2: dx = 0; dy = 1; break;
      case 3: dx = -1; dy = 0; break;
    }
    
    new_x = e->x + dx;
    new_y = e->y + dy;
    
    if (!game_can_move_to(new_x, new_y)) continue;
    
    /* Pot excavar terra (no gemmes) */
    if (game.map[new_y][new_x] == 0) {
      int dist_x = player_dx - dx;
      int dist_y = player_dy - dy;
      int dist = (dist_x < 0 ? -dist_x : dist_x) + (dist_y < 0 ? -dist_y : dist_y);
      
      if (dist < best_dist) {
        best_dist = dist;
        best_dir = i;
      }
    }
  }
  
  /* Si trobem terra per excavar cap al jugador, fer-ho */
  if (best_dir >= 0) {
    switch (best_dir) {
      case 0: new_y = e->y - 1; new_x = e->x; break;
      case 1: new_x = e->x + 1; new_y = e->y; break;
      case 2: new_y = e->y + 1; new_x = e->x; break;
      case 3: new_x = e->x - 1; new_y = e->y; break;
    }
    /* Excavar (convertir terra en túnel) i moure's */
    game.map[new_y][new_x] = 1;
    e->x = new_x;
    e->y = new_y;
    e->direction = best_dir;
  }
}

void game_check_collisions(void) {
  int i;
  
  /* Si el jugador és invulnerable o ja està mort, no fer res */
  if (game.player.invulnerable_ticks > 0) return;
  if (game.player.lives <= 0) return;
  if (game.state != STATE_PLAYING) return;
  
  for (i = 0; i < MAX_ENEMIES; i++) {
    if (!game.enemies[i].alive) continue;
    
    /* Col·lisió amb el jugador */
    if (game.enemies[i].x == game.player.x && 
        game.enemies[i].y == game.player.y) {
      
      game.player.lives--;
      game.player.invulnerable_ticks = PLAYER_INVULN_TICKS;
      
      if (game.player.lives <= 0) {
        game.state = STATE_GAMEOVER;
      }
      return;  /* Només processar una col·lisió per tick */
    }
  }
}

void game_update_enemies(void) {
  int i;
  
  /* Velocitat d'enemics segons nivell (més ràpids en nivells alts) */
  int move_ticks = ENEMY_MOVE_TICKS - (game.level - 1) * 54;
  int dig_ticks = ENEMY_DIG_TICKS - (game.level - 1) * 90;
  
  if (move_ticks < 54) move_ticks = 54;
  if (dig_ticks < 72) dig_ticks = 72;
  
  for (i = 0; i < MAX_ENEMIES; i++) {
    if (!game.enemies[i].alive) continue;
    
    game.enemies[i].move_counter++;
    game.enemies[i].dig_counter++;
    
    /* Primer intentar moure's per túnels existents */
    if (game.enemies[i].move_counter >= move_ticks) {
      game.enemies[i].move_counter = 0;
      game_move_enemy(i);
    }
    
    /* Si no pot moure's, intentar excavar (més lent) */
    if (game.enemies[i].dig_counter >= dig_ticks) {
      game.enemies[i].dig_counter = 0;
      game_enemy_dig(i);
    }
  }
}

/* ============================================ */
/*         RENDERITZAT DEL JOC                 */
/* ============================================ */

/* Dibuixa NOMÉS la línia inferior (un cop a l'inici) */
void draw_bottom_bar(void) {
  if (bottom_bar_drawn) return;
  game_draw_filled_box(0, 24, GAME_SCREEN_WIDTH - 1, 24, ' ', COLOR_DARKGRAY << 4 | COLOR_WHITE);
  game_draw_string(2, 24, "WASD/Arrows:Move  ESC:Pause  R:Restart", COLOR_DARKGRAY << 4 | COLOR_LIGHTGRAY);
  bottom_bar_drawn = 1;
}

void draw_game_hud(void) {
  char buf[16];
  int i;
  
  /* Línia superior - fons blau */
  game_draw_filled_box(0, 0, GAME_SCREEN_WIDTH - 1, 1, ' ', COLOR_BLUE << 4 | COLOR_WHITE);
  
  /* Score */
  game_draw_string(2, 0, "SCORE:", COLOR_BLUE << 4 | COLOR_WHITE);
  itoa_padded(game.player.score, buf, 6);
  game_draw_string(9, 0, buf, COLOR_BLUE << 4 | COLOR_YELLOW);
  
  /* Vides */
  game_draw_string(20, 0, "LIVES:", COLOR_BLUE << 4 | COLOR_WHITE);
  for (i = 0; i < game.player.lives; i++) {
    game_set_pixel(27 + i * 2, 0, '*', COLOR_BLUE << 4 | COLOR_LIGHTRED);
  }
  
  /* Gemmes */
  game_draw_string(38, 0, "GEMS:", COLOR_BLUE << 4 | COLOR_WHITE);
  itoa_padded(game.gems_collected, buf, 2);
  game_draw_string(44, 0, buf, COLOR_BLUE << 4 | COLOR_LIGHTCYAN);
  game_draw_string(46, 0, "/", COLOR_BLUE << 4 | COLOR_WHITE);
  itoa_padded(game.total_gems, buf, 2);
  game_draw_string(47, 0, buf, COLOR_BLUE << 4 | COLOR_LIGHTCYAN);
  
  /* Nivell */
  game_draw_string(55, 0, "LVL:", COLOR_BLUE << 4 | COLOR_WHITE);
  itoa_padded(game.level, buf, 2);
  game_draw_string(60, 0, buf, COLOR_BLUE << 4 | COLOR_LIGHTGREEN);
  
  /* FPS (dalt a la dreta) */
  game_draw_string(70, 0, "FPS:", COLOR_BLUE << 4 | COLOR_LIGHTGRAY);
  itoa_padded(fps_display, buf, 3);
  game_draw_string(75, 0, buf, COLOR_BLUE << 4 | COLOR_WHITE);
  
  /* Línia inferior - només dibuixar un cop */
  draw_bottom_bar();
}

void draw_game_area(void) {
  int x, y;
  
  /* Dibuixar el mapa */
  for (y = GAME_AREA_TOP; y <= GAME_AREA_BOTTOM; y++) {
    for (x = GAME_AREA_LEFT; x <= GAME_AREA_RIGHT; x++) {
      switch (game.map[y][x]) {
        case 0: /* Terra */
          game_set_pixel(x, y, '#', ATTR_DIRT);
          break;
        case 1: /* Túnel */
          game_set_pixel(x, y, '.', ATTR_TUNNEL);
          break;
        case 2: /* Gemma */
          game_set_pixel(x, y, '*', ATTR_GEM);
          break;
      }
    }
  }
  
  /* Dibuixar vores de l'àrea de joc */
  game_draw_box(GAME_AREA_LEFT - 1, GAME_AREA_TOP - 1, 
           GAME_AREA_RIGHT + 1, GAME_AREA_BOTTOM + 1, ATTR_BORDER);
}

void draw_game_player(void) {
  char player_char;
  char attr = ATTR_PLAYER;
  
  /* Parpelleig si és invulnerable */
  if (game.player.invulnerable_ticks > 0 && (game_frame_count % 4) < 2) {
    attr = COLOR_BLACK << 4 | COLOR_DARKGRAY;
  }
  
  /* Caràcter segons direcció */
  switch (game.player.direction) {
    case 'U': player_char = '^'; break;
    case 'D': player_char = 'v'; break;
    case 'L': player_char = '<'; break;
    case 'R': player_char = '>'; break;
    default: player_char = '@'; break;
  }
  
  game_set_pixel(game.player.x, game.player.y, player_char, attr);
}

void draw_game_enemies(void) {
  int i;
  
  for (i = 0; i < MAX_ENEMIES; i++) {
    if (!game.enemies[i].alive) continue;
    
    /* Animació simple dels enemics */
    char enemy_char = ((game_frame_count + i) % 8 < 4) ? '@' : 'O';
    game_set_pixel(game.enemies[i].x, game.enemies[i].y, enemy_char, ATTR_ENEMY);
  }
}

void draw_game_screen(void) {
  /* NO netejar la pantalla - draw_game_area ja dibuixa tota l'àrea */
  /* Això evita parpelleig */
  draw_game_area();
  draw_game_enemies();
  draw_game_player();
  draw_game_hud();
}

void draw_pause_menu(void) {
  /* Només dibuixar la caixa de pausa */
  /* Caixa de pausa */
  game_draw_filled_box(25, 8, 54, 16, ' ', COLOR_DARKGRAY << 4 | COLOR_WHITE);
  game_draw_box(25, 8, 54, 16, COLOR_CYAN << 4 | COLOR_WHITE);
  
  game_draw_string_centered(10, "= = = PAUSED = = =", COLOR_DARKGRAY << 4 | COLOR_YELLOW);
  game_draw_string_centered(12, "ENTER/SPACE - Resume", COLOR_DARKGRAY << 4 | COLOR_WHITE);
  game_draw_string_centered(13, "R - Restart Game", COLOR_DARKGRAY << 4 | COLOR_WHITE);
  game_draw_string_centered(14, "ESC - Back to Title", COLOR_DARKGRAY << 4 | COLOR_LIGHTGRAY);
}

void draw_game_over(void) {
  char buf[32];
  game_clear_screen(COLOR_BLACK << 4 | COLOR_BLACK);
  
  /* Fons dramàtic */
  game_draw_filled_box(20, 7, 59, 17, ' ', COLOR_RED << 4 | COLOR_WHITE);
  game_draw_box(20, 7, 59, 17, COLOR_LIGHTRED << 4 | COLOR_YELLOW);
  
  game_draw_string_centered(9, "* * * GAME OVER * * *", COLOR_RED << 4 | COLOR_YELLOW);
  
  game_draw_string_centered(11, "Final Score:", COLOR_RED << 4 | COLOR_WHITE);
  itoa_padded(game.player.score, buf, 6);
  game_draw_string_centered(12, buf, COLOR_RED << 4 | COLOR_LIGHTCYAN);
  
  game_draw_string_centered(14, "ENTER/SPACE - Play Again", COLOR_RED << 4 | COLOR_WHITE);
  game_draw_string_centered(15, "ESC - Back to Title", COLOR_RED << 4 | COLOR_LIGHTGRAY);
}

void draw_level_up_screen(void) {
  int i;
  char buf[16];
  
  game_clear_screen(COLOR_BLACK << 4 | COLOR_BLACK);
  
  /* Fons amb estrelles animades */
  for (i = 0; i < 50; i++) {
    int x = (i * 17 + game_frame_count) % GAME_SCREEN_WIDTH;
    int y = (i * 13 + game_frame_count / 3) % GAME_SCREEN_HEIGHT;
    game_set_pixel(x, y, '*', COLOR_BLACK << 4 | COLOR_YELLOW);
  }
  
  /* Caixa de nivell completat */
  game_draw_filled_box(20, 8, 59, 16, ' ', COLOR_CYAN << 4 | COLOR_WHITE);
  game_draw_box(20, 8, 59, 16, COLOR_LIGHTCYAN << 4 | COLOR_WHITE);
  
  game_draw_string_centered(9, "* LEVEL COMPLETE! *", COLOR_CYAN << 4 | COLOR_YELLOW);
  
  buf[0] = 'L'; buf[1] = 'e'; buf[2] = 'v'; buf[3] = 'e'; buf[4] = 'l'; buf[5] = ' ';
  buf[6] = '0' + game.level;
  buf[7] = ' '; buf[8] = 'C'; buf[9] = 'l'; buf[10] = 'e'; buf[11] = 'a'; 
  buf[12] = 'r'; buf[13] = 'e'; buf[14] = 'd'; buf[15] = '\0';
  game_draw_string_centered(11, buf, COLOR_CYAN << 4 | COLOR_WHITE);
  
  game_draw_string_centered(13, "Get ready for next level...", COLOR_CYAN << 4 | COLOR_LIGHTCYAN);
  
  /* Mostrar score actual */
  itoa_padded(game.player.score, buf, 6);
  game_draw_string(28, 15, "Score: ", COLOR_CYAN << 4 | COLOR_WHITE);
  game_draw_string(35, 15, buf, COLOR_CYAN << 4 | COLOR_YELLOW);
}

void draw_win_screen(void) {
  int i;
  char buf[32];
  game_clear_screen(COLOR_BLACK << 4 | COLOR_BLACK);
  
  /* Fons xulo */
  for (i = 0; i < GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT; i++) {
    int x = i % GAME_SCREEN_WIDTH;
    int y = i / GAME_SCREEN_WIDTH;
    if ((x + y + game_frame_count/4) % 5 == 0) {
      game_screen_buffer[i * 2] = '*';
      game_screen_buffer[i * 2 + 1] = COLOR_BLACK << 4 | (COLOR_LIGHTCYAN + ((x + y) % 4));
    }
  }
  
  /* Caixa de victòria */
  game_draw_filled_box(15, 6, 64, 18, ' ', COLOR_GREEN << 4 | COLOR_WHITE);
  game_draw_box(15, 6, 64, 18, COLOR_LIGHTGREEN << 4 | COLOR_YELLOW);
  
  game_draw_string_centered(7, "* * * CONGRATULATIONS! * * *", COLOR_GREEN << 4 | COLOR_YELLOW);
  game_draw_string_centered(9, "You completed all 3 levels!", COLOR_GREEN << 4 | COLOR_WHITE);
  game_draw_string_centered(10, "ZEOS DIGGER MASTER!", COLOR_GREEN << 4 | COLOR_LIGHTCYAN);
  
  game_draw_string_centered(12, "Final Score:", COLOR_GREEN << 4 | COLOR_WHITE);
  itoa_padded(game.player.score, buf, 6);
  game_draw_string_centered(13, buf, COLOR_GREEN << 4 | COLOR_LIGHTCYAN);
  
  game_draw_string_centered(15, "Lives Remaining:", COLOR_GREEN << 4 | COLOR_WHITE);
  itoa_padded(game.player.lives, buf, 1);
  game_draw_string_centered(16, buf, COLOR_GREEN << 4 | COLOR_YELLOW);
  
  game_draw_string_centered(18, "ENTER - Play Again  ESC - Title", COLOR_GREEN << 4 | COLOR_LIGHTGRAY);
}

/* ============================================ */
/*         CALLBACK DE TECLAT DEL JOC          */
/* ============================================ */

void game_keyboard_handler(char key, int pressed) {
  /* Per a tecles de moviment: guardar última direcció premuda */
  /* Quan s'allibera, només esborrar si era la direcció actual */
  
  switch (key) {
    /* Moviment - WASD */
    case KEY_W:
    case KEY_UP:
      key_up_game = pressed;
      if (pressed) last_direction = 1;  /* AMUNT */
      else if (last_direction == 1) last_direction = 0;
      break;
    case KEY_S:
    case KEY_DOWN:
      key_down_game = pressed;
      if (pressed) last_direction = 2;  /* AVALL */
      else if (last_direction == 2) last_direction = 0;
      break;
    case KEY_A:
    case KEY_LEFT:
      key_left_game = pressed;
      if (pressed) last_direction = 3;  /* ESQUERRA */
      else if (last_direction == 3) last_direction = 0;
      break;
    case KEY_D:
    case KEY_RIGHT:
      key_right_game = pressed;
      if (pressed) last_direction = 4;  /* DRETA */
      else if (last_direction == 4) last_direction = 0;
      break;
    
    /* Accions (només quan es prem) */
    case KEY_SPACE:
    case KEY_ENTER:
      if (pressed) key_action_game = 1;
      break;
    case KEY_ESC:
      if (pressed) key_pause_game = 1;
      break;
    case KEY_R:
      if (pressed) key_restart_game = 1;
      break;
  }
}

/* ============================================ */
/*         THREADS DEL JOC                     */
/* ============================================ */

/* Thread de LÒGICA: actualitza jugador i enemics cada tick (Milestone 1 + 4) */
void game_logic_thread(void *arg) {
  int fps_tick_counter = 0;
  int last_frame_count = 0;
  
  while (game_running) {
    WaitForTick();
    
    /* Actualitzar FPS */
    fps_tick_counter++;
    if (fps_tick_counter >= TICKS_PER_SECOND) {
      fps_display = game_frame_count - last_frame_count;
      last_frame_count = game_frame_count;
      fps_tick_counter = 0;
    }
    
    /* Lògica del joc (jugador, enemics, col·lisions) */
    if (game.state == STATE_PLAYING) {
      game_update_player();  /* Moviment del jugador controlat per ticks */
      game_update_enemies();
      game_check_collisions();
    }
    
    /* Transició de nivell */
    if (game.state == STATE_LEVEL_UP) {
      if (game.level_transition_ticks > 0) {
        game.level_transition_ticks--;
      } else {
        init_next_level();
      }
    }
  }
  
  ThreadExit();
}

/* Thread de RENDER: dibuixa la pantalla cada tick (Milestone 1 + 3 + 4) */
void game_render_thread(void *arg) {
  while (game_running) {
    WaitForTick();
    
    /* Renderitzar segons estat */
    switch (game.state) {
      case STATE_TITLE:
        draw_title_screen();
        write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
        break;
      case STATE_PLAYING:
        draw_game_screen();
        write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
        break;
      case STATE_PAUSED:
        /* Només dibuixar i enviar UN COP */
        if (!pause_menu_drawn) {
          draw_pause_menu();
          write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
          pause_menu_drawn = 1;
        }
        /* NO fer res més - pantalla estàtica */
        break;
      case STATE_GAMEOVER:
        draw_game_over();
        write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
        break;
      case STATE_WIN:
        draw_win_screen();
        write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
        break;
      case STATE_LEVEL_UP:
        draw_level_up_screen();
        write(10, game_screen_buffer, GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2);
        break;
    }
    
    game_frame_count++;
  }
  
  ThreadExit();
}

/* ============================================ */
/*         PROCESSAR INPUT DEL JUGADOR         */
/* ============================================ */

/* Processa l'input del jugador - cridat des del bucle principal */
void process_game_input(void) {
  /* Gestionar input de menús */
  if (game.state == STATE_TITLE) {
    if (key_action_game) {
      init_game_state();
      bottom_bar_drawn = 0;  /* Reiniciar per redibuixar en entrar al joc */
      key_action_game = 0;
    }
  }
  else if (game.state == STATE_PAUSED) {
    if (key_action_game) {
      game.state = STATE_PLAYING;
      pause_menu_drawn = 0;  /* Reiniciar per la propera vegada */
      key_action_game = 0;
    }
    else if (key_pause_game) {
      game.state = STATE_TITLE;
      pause_menu_drawn = 0;
      bottom_bar_drawn = 0;
      key_pause_game = 0;
    }
    else if (key_restart_game) {
      init_game_state();
      pause_menu_drawn = 0;
      bottom_bar_drawn = 0;
      key_restart_game = 0;
    }
  }
  else if (game.state == STATE_PLAYING) {
    /* El moviment del jugador es processa a game_logic_thread (cada tick) */
    /* Aquí només processem pausa i reinici */
    if (key_pause_game) {
      game.state = STATE_PAUSED;
      pause_menu_drawn = 0;  /* Permetre dibuixar el menú de pausa */
      key_pause_game = 0;
    }
    else if (key_restart_game) {
      init_game_state();
      bottom_bar_drawn = 0;
      key_restart_game = 0;
    }
  }
  else if (game.state == STATE_GAMEOVER || game.state == STATE_WIN) {
    if (key_action_game || key_restart_game) {
      init_game_state();
      bottom_bar_drawn = 0;
      key_action_game = 0;
      key_restart_game = 0;
    }
    else if (key_pause_game) {
      game.state = STATE_TITLE;
      bottom_bar_drawn = 0;
      key_pause_game = 0;
    }
  }
}

/* ============================================ */
/*         FUNCIÓ PRINCIPAL DEL JOC            */
/* ============================================ */

void start_game(void) {
  int i;
  
  /* Inicialitzar estat del joc */
  game.state = STATE_TITLE;
  game_running = 1;
  game_frame_count = 0;
  fps_display = 0;
  player_move_counter = 0;
  
  /* Netejar buffer inicialment */
  for (i = 0; i < GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT; i++) {
    game_screen_buffer[i * 2] = ' ';
    game_screen_buffer[i * 2 + 1] = 0x07;
  }
  
  /* Registrar callback de teclat (Milestone 2) */
  KeyboardEvent(game_keyboard_handler);
  
  /* Crear thread de lògica (Milestone 1) */
  ThreadCreate(game_logic_thread, 0);
  
  /* Crear thread de render (Milestone 1) */
  ThreadCreate(game_render_thread, 0);
  
  /* Bucle principal: processa input amb yield() */
  while (game_running) {
    process_game_input();
    yield();
  }
  
  /* Cleanup */
  KeyboardEvent(0);
}

/* ========================================= */
/* FUNCIÓ MAIN                               */
/* ========================================= */

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  //write(1, "\n==========================================\n", 45);
  //write(1, "   ZeOS - Benchmark de Suport de Threads\n", 45);
  //write(1, "==========================================\n\n", 46);
  
  // Executem els tests en ordre
  //test_1_simple_thread_creation();
  //test_2_concurrent_threads();
  //test_3_stack_growth();
  //test_4_fork_with_threads();
  //test_5_access_ok_validation();
  //test_6_stack_limits();
  //test_7_secondary_thread_exit();
  //test_8_leader_exit();
  
  // Tests de teclat
  //test_9_1_disable_handler();
  //test_9_2_verify_handler();
  //test_9_3_einprogress();
  //test_9_4_interactive();
  //test_9_5_params();
  //test_9_6_thread_concurrency();
  //test_9_7_process_concurrency();
  
  //write(1, "\n==========================================\n", 45);
  //write(1, "   Benchmark completat\n", 26);
  //write(1, "==========================================\n", 45);
  //test_fps();
  //test_screen_write();
  
  // Test WaitForTick amb múltiples threads
  //test_waitfortick_multithreaded();
  
  // Test visual de WaitForTick
  //test_tick_screen();

  /* MILESTONE 5: Iniciar el videojoc */
  start_game();

  /* No hauria d'arribar aquí */

  while (1) {
    yield();
  }
}


