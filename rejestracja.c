#include "common.h"
void obsluga_biletomatu(int id_msg, int count) { 
    Komunikat msg; 
    // Biletomat przetwarza tyle biletów, ile jest aktywnych stanowisk w cyklu 
    for(int i=0; i<count; i++) { 
        if (msgrcv(id_msg, &msg, sizeof(Komunikat) - sizeof(long), 1, IPC_NOWAIT) != -1) { 
            msg.mtype = msg.pid_petenta; msgsnd(id_msg, &msg, sizeof(Komunikat) - sizeof(long), 0); 
        } 
    } 
}

void rejestracja_manager(int shm_id, int sem_id) { 
    SharedData *shm = (SharedData*)shmat(shm_id, NULL, 0); 
    int msg_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0600); 
    printf("[REJESTRACJA] System biletowy aktywny.\n");
}