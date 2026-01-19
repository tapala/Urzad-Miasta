#include "common.h"
#include <signal.h>
#include <sys/wait.h>

#define MAX_KASY 3

pid_t kasy[MAX_KASY];
int liczba_kas = 0;
int shmid, semid;
SharedData *shm;

void kasa_loop(){
    int msg_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0);
    int msg_back_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0);
    Komunikat msg;

    while (1) {
        if (msgrcv(msg_id, &msg, sizeof(Komunikat) - sizeof(long), 1, 0) == -1){
            //fprintf(stderr,"%s -- %d -- Wyjebka na msgqueue - %s => %d \n", strerror(errno), getpid(),__FILE__,__LINE__);
            continue;
        }
        msg.mtype = msg.pid_petenta;
        
        // Zmniejszenie limitu w SHM
        sem_p(semid, SEM_MUTEX);
        if(shm->limity_przyjec[msg.typ_sprawy] > 0)
            shm->limity_przyjec[msg.typ_sprawy]--;
        else 
            msg.typ_sprawy = LIMIT_OSIAGNIETY;
        sem_v(semid, SEM_MUTEX);

        if(msgsnd(msg_back_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            //fprintf(stderr,"%s -- %d -- Wyjebka na msgqueue - %s => %d \n", strerror(errno), getpid(),__FILE__,__LINE__);
        }
    };
}


void uruchom_kase(){
    if (liczba_kas >= MAX_KASY)
        return;

    // Rejestracja robi podział komórkowy i rozkazuje bachorowi dymać na kasie
    pid_t pid = fork();
    if (pid == -1){
        perror("Fork error");
        return;
    }
    else if (pid == 0) {
        kasa_loop();
        exit(0);
    }

    // Inkrement kas i zapisanie do sigkilla
    kasy[liczba_kas++] = pid;
}


void zamknij_kase() {
    if (liczba_kas <= 0)
        return;

    // Dekrement i wczytanie pidu do auto-kasacji
    pid_t pid = kasy[--liczba_kas];
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    printf("Kasa zamknieta \n");
}


int main() {
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    semid = semget(ftok(FTOK_PATH, ID_SEM), 4, 0);

    shm = (SharedData*)shmat(shmid, NULL, 0);

    printf("[REJESTRACJA] System biletowy uruchomiony\n");

    while (1) {

        // Wcztanie z pamięci współdzielonej
        sem_p(semid, SEM_MUTEX);
        int kolejka = shm->kolejka_do_biletow;
        int status = shm->koniec_pracy;
        sem_v(semid, SEM_MUTEX);

        // Ewakuacja
        if (status == 2) {
            break;
        }

        // Wyliczenie K
        int N = MAX_PETENTOW_W_BUDYNKU;
        int K = N / 3;
        if (K < 1) K = 1;

        // Ustalenie liczby docelowej kas
        int docelowe = 1;
        if (kolejka > 2 * K) docelowe = 3;
        else if (kolejka > K) docelowe = 2;

        // Histereza(swoją drogą bardzo fajne słówko) kas
        if (liczba_kas == 3 && kolejka < (2 * N) / 3)
            docelowe = 2;

        if (liczba_kas == 2 && kolejka < K)
            docelowe = 1;

        // Nadgonienie liczby otwartych kas do liczby docelowej
        while (liczba_kas < docelowe)
            uruchom_kase();

        /while (liczba_kas > docelowe)
            zamknij_kase();

    }

    // Zamknięcie wszystkiego
    while (liczba_kas > 0)
        zamknij_kase();

    shmdt(shm);
    printf("[REJESTRACJA] Zamykanie systemu biletowego\n");
    return 0;
}