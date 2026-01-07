#include "common.h"

// Funkcje semaforów 
void sem_op(int semid, int sem_num, int op) { 
    struct sembuf s; 
    s.sem_num = sem_num; 
    s.sem_op = op; 
    s.sem_flg = 0; 
    if (semop(semid, &s, 1) == -1) 
        if (errno != EIDRM && errno != EINVAL) {
            perror("semop");
        }
    } 
void sem_p(int semid, int sem_num) { sem_op(semid, sem_num, -1); } 
void sem_v(int semid, int sem_num) { sem_op(semid, sem_num, 1); }
// Logowanie do pliku
void log_to_file(const char *msg) { 
    FILE *f = fopen("raport.txt", "a"); 
    if (f) { 
        fprintf(f, "%s", msg); 
        fclose(f); } 
    }