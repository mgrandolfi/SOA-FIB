/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */
 
#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>

// Retorna l'identificador de pila del thread actual
int get_stack_id(void);

// Defineix errno de forma thread-safe usant una ubicació de memòria per thread, evita race conditions
int *__errno_location(void);
#define errno (*__errno_location())

int write(int fd, char *buffer, int size);

void itoa(int a, char *b);

int strlen(char *a);

void perror();

int getpid();

int fork();

void exit();

int yield();

int get_stats(int pid, struct stats *st);

int ThreadCreate(void (*function)(void *), void *parameter);
void ThreadExit(void);

// Registra una funcio de callback per esdeveniments de teclat
int KeyboardEvent(void (*func)(char key, int pressed));

int gettime();

int WaitForTick();

#endif  /* __LIBC_H__ */
