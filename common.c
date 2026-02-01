#include "common.h"

static sigset_t old_sigset, new_sigset;

// Funkcje semaforów 
int sem_op(int semid, int sem_num, int op, short flag) { 
    struct sembuf s; 
    s.sem_num = sem_num; 
    s.sem_op = op; 
    s.sem_flg = flag; 
    if (semop(semid, &s, 1) == -1){
        if (errno == EINTR)
        {
            return -1;
        }
        else if (errno != EIDRM && errno != EINVAL) {
            fprintf(stderr,"%s -- %s -- %d; Semop %d\n", strerror(errno),__FILE__,__LINE__, sem_num);
            exit(1);
        }
    }
    return 0;
} 
int sem_p(int semid, int sem_num) { return sem_op(semid, sem_num, -1, 0); } 
int sem_v(int semid, int sem_num) { return sem_op(semid, sem_num, 1, 0); }

void log_to_file(const char *msg, int semid) {
    while(sem_p(semid, SEM_LOG_MUTEX)); 
    FILE *f = fopen("raport.txt", "a"); 
    if (f) { 
        fprintf(f, "%s", msg); 
        fclose(f); 
    } 
    else{
        while(sem_v(semid, SEM_LOG_MUTEX)); 
        perror("fopen");
        exit(1);
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

key_t ftok_handler(const char *path, int proj_id){
    key_t result = ftok(path, proj_id);
    if(result == -1){
        perror("ftok");
        exit(1);
    }
    return result;
}

int msgget_CREATE_handler(const char *path, int proj_id){
    int result = msgget(ftok_handler(path, proj_id), 0600 | IPC_CREAT);
    if(result == -1){
        perror("msgget");
        exit(1);
    }
    return result;
}

void semctl_SETVAL_handler(int semid, int sem_name,int value){
    if(semctl(semid, sem_name, SETVAL, value) == -1){
        perror("semctl SETVAL");
        exit(1);
    }

}

void msgctl_IPC_RMID_handler(int msqid){
    if(msgctl(msqid, IPC_RMID, NULL) == -1){
        perror("msgctl IPC_RMID");
        exit(1);
    }
}

void init_signals(void) {
    sigemptyset(&new_sigset);
    sigaddset(&new_sigset, SIGRTMIN);
}

void block_signal(void) {
    if (pthread_sigmask(SIG_BLOCK, &new_sigset, &old_sigset) != 0) {
        perror("pthread_sigmask block");
    }
}

void restore_signal(void) {
    if (pthread_sigmask(SIG_SETMASK, &old_sigset, NULL) != 0) {
        perror("pthread_sigmask restore");
    }
}

void sem_p_mutex(int semid) {
    block_signal();
    while(sem_p(semid, SEM_MUTEX));
}

void sem_v_mutex(int semid) {
    while(sem_v(semid, SEM_MUTEX));
    restore_signal();
}