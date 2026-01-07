#ifndef COMMON_H 
#define COMMON_H

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/sem.h> 
#include <sys/msg.h> 
#include <signal.h> 
#include <errno.h> 
#include <time.h> 
#include <string.h> 
#include <sys/wait.h>

// Deklaracje funkcji pomocniczych 
void log_to_file(const char *msg); 
void sem_p(int semid, int sem_num); 
void sem_v(int semid, int sem_num); 
void sem_op(int semid, int sem_num, int op);
#endif