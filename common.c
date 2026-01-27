#include "common.h"

// Funkcje semaforów 
void sem_op(int semid, int sem_num, int op, short flag) { 
    struct sembuf s; 
    s.sem_num = sem_num; 
    s.sem_op = op; 
    s.sem_flg = flag; 
    if (semop(semid, &s, 1) == -1){
        if (errno != EIDRM && errno != EINVAL) {
            perror("semop");
        }
    }
} 
void sem_p(int semid, int sem_num) { sem_op(semid, sem_num, -1, 0); } 
void sem_v(int semid, int sem_num) { sem_op(semid, sem_num, 1, 0); }

int semt_op(int semid, int sem_num, int op, long nanotime) { 
    struct sembuf s; 
    s.sem_num = sem_num; 
    s.sem_op = op; 
    s.sem_flg = 0; 
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = nanotime;
    if (semtimedop(semid, &s, 1, &t) == -1){
        if(errno == EAGAIN)
            return -1;
        if (errno != EIDRM && errno != EINVAL) {
            perror("semop");
            exit(1);
        }
    }
    return 0;
} 
int semt_p(int semid, int sem_num, long nanotime) { return semt_op(semid, sem_num, -1, nanotime); } 
int semt_v(int semid, int sem_num, long nanotime) { return semt_op(semid, sem_num, 1, nanotime); }
// Logowanie do pliku
void log_to_file(const char *msg, int semid) {
    sem_p(semid, SEM_LOG_MUTEX); 
    FILE *f = fopen("raport.txt", "a"); 
    if (f) { 
        fprintf(f, "%s", msg); 
        fclose(f); 
    } 
    sem_v(semid, SEM_LOG_MUTEX); 
}

void gotowanie_procesora(int seconds){
    time_t start = time(NULL);
    while(1){
        time_t now = time(NULL);
        if((now - start) >= seconds)
            break;
    }
}