#include "common.h"

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