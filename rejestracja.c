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

    while (1) { 
        sem_p(sem_id, SEM_MUTEX); 

        if (shm->koniec_pracy == 2) { 
            sem_v(sem_id, SEM_MUTEX); 
            break; 
        } 

        // Wyciągamy liczbę w kolejce(do aktywacji biletomatów)
        int kolejka = shm->kolejka_do_biletow; 
        int N = MAX_PETENTOW_W_BUDYNKU; 
        int K = N / 3; 
        if (K < 1) K = 1; 
        
        // Skalowanie 
        int aktywne = 1; 
        if (kolejka > 2 * K) aktywne = 3; 
        else if (kolejka > K) aktywne = 2;
        
        // Histereza aktywacji biletomatów
        if (aktywne == 3 && kolejka < (2.0/3.0)*N) aktywne = 2; 
        if (aktywne == 2 && kolejka < K) aktywne = 1; 

        shm->liczba_aktywnych_biletomatow = aktywne; 
        sem_v(sem_id, SEM_MUTEX); 
        obsluga_biletomatu(msg_id, aktywne); 
        usleep(200000); 
        // Cykl pracy maszyny 
        } 

        shmdt(shm);
}