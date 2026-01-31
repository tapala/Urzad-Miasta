#include "common.h"
#include <signal.h>
#include <sys/wait.h>

#define MAX_BILETOMATY 3

pid_t kasy[MAX_BILETOMATY];
int liczba_biletomatow = 0;
int nr_biletu = 0;
int run_flag = 1;
int shmid, semid;
SharedData *shm;

void signal_handler(int sig);

void biletomat_loop(){
    int msg_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0);
    int msg_back_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0);
    Komunikat msg;

    while (run_flag) {
        if (msgrcv(msg_id, &msg, sizeof(Komunikat) - sizeof(long), 1, 0) == -1){
            if(errno == EINTR){
                continue;
            }
            else{
                fprintf(stderr,"%s -- %d -- Wyjebka na msgrcv - %s => %d \n", strerror(errno), getpid(),__FILE__,__LINE__);
                exit(1);
            }
        }
        msg.mtype = msg.pid_petenta;
        
        // Zmniejszenie limitu w SHM
        while(sem_p(semid, SEM_MUTEX));
        if(shm->limity_przyjec[msg.typ_sprawy] > 0){
            shm->limity_przyjec_sum--;
            shm->limity_przyjec[msg.typ_sprawy]--;
            msg.nr_biletu = MAX_BILETOW - shm->limity_przyjec_sum;
        }
        else 
            msg.typ_sprawy = LIMIT_OSIAGNIETY;

        while(sem_v(semid, SEM_MUTEX));

        try_again:
        if(msgsnd(msg_back_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            if(errno == EINTR){
                goto try_again;
            }
            else{
                fprintf(stderr,"%s -- %d -- Wyjebka na msgsnd - %s => %d \n", strerror(errno), getpid(),__FILE__,__LINE__);
            }
        }
    }
    exit(1);
}


void uruchom_biletomat(){
    if (liczba_biletomatow >= MAX_BILETOMATY)
        return;

    // Rejestracja robi podział komórkowy i rozkazuje bachorowi dymać na kasie
    pid_t pid = fork();
    printf("OTWIERAM BILETOMAT \n");
    fflush(stdout);
    if (pid == -1){
        perror("Fork error");
        return;
    }
    else if (pid == 0) {
        signal(SIGTERM, signal_handler);
        biletomat_loop();
        exit(0);
    }

    // Inkrement kas i zapisanie do sigkilla
    kasy[liczba_biletomatow++] = pid;
}


void zamkni_biletomat() {
    if (liczba_biletomatow <= 0)
        return;

    // Dekrement i wczytanie pidu do auto-kasacji
    pid_t pid = kasy[--liczba_biletomatow];
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    printf("BILETOMAT ZAMKNIETY \n");
    fflush(stdout);
}


int main() {
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    semid = semget(ftok(FTOK_PATH, ID_SEM), 6, 0);
    signal(SIGRTMIN, signal_handler);
    shm = (SharedData*)shmat(shmid, NULL, 0);

    printf("[REJESTRACJA] System biletowy uruchomiony\n");

    while (1) {

        // Wcztanie z pamięci współdzielonej
        while(sem_p(semid, SEM_MUTEX));
        int kolejka = shm->kolejka_do_biletow;
        int status = shm->koniec_pracy;
        while(sem_v(semid, SEM_MUTEX));

        // Ewakuacja
        if (status == 3) {
            break;
        }

        // Wyliczenie K
        int N = MAX_PETENTOW_W_BUDYNKU;
        int K = N / 3;
        if (K < 1) K = 1;

        // Ustalenie liczby docelowej kas
        int docelowe = 1;
        if (kolejka > 2 * PROG_URUCHOMIENIA_KAS) docelowe = 3;
        else if (kolejka > PROG_URUCHOMIENIA_KAS) docelowe = 2;

        // Histereza(swoją drogą bardzo fajne słówko) kas
        if (liczba_biletomatow == 3 && kolejka < (2 * N) / 3)
            docelowe = 2;

        if (liczba_biletomatow == 2 && kolejka < K)
            docelowe = 1;

        // Nadgonienie liczby otwartych kas do liczby docelowej
        while (liczba_biletomatow < docelowe)
            uruchom_biletomat();

        while (liczba_biletomatow > docelowe)
            zamkni_biletomat();

    }

    // Zamknięcie wszystkiego
    while (liczba_biletomatow > 0)
        zamkni_biletomat();

    while (wait(NULL) > 0); 
    shmdt(shm);
    printf("[REJESTRACJA] Zamykanie systemu biletowego\n");
    return 0;
}


void signal_handler(int sig) {
    if (sig == SIGTERM){
        run_flag = 0;
    }
}