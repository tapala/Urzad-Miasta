#include "common.h"
#include <signal.h>

#define MAX_KASY 3

pid_t kasy[MAX_KASY];
int liczba_kas = 0;

void uruchom_kase(){
    // Uruchomienie kasy
}


void zamknij_kase() {
    // Zamykanie kasy
}

void kasa_loop(){
    // Pętla pracy kasy
}

int main() {
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    int semid = semget(ftok(FTOK_PATH, ID_SEM), 2, 0);

    SharedData *shm = shmat(shmid, NULL, 0);

    printf("[REJESTRACJA] System biletowy uruchomiony\n");

    while (1) {
        sem_p(semid, SEM_MUTEX);
        int kolejka = shm->kolejka_do_biletow;
        int status = shm->koniec_pracy;
        sem_v(semid, SEM_MUTEX);

        if (status == 2) {
            break;
        }

        int N = MAX_PETENTOW_W_BUDYNKU;
        int K = N / 3;
        if (K < 1) K = 1;

        int docelowe = 1;
        if (kolejka > 2 * K) docelowe = 3;
        else if (kolejka > K) docelowe = 2;

        // Histereza(swoją drogą bardzo fajne słówko) kas
        if (liczba_kas == 3 && kolejka < (2 * N) / 3)
            docelowe = 2;

        if (liczba_kas == 2 && kolejka < K)
            docelowe = 1;

        while (liczba_kas < docelowe)
            uruchom_kase();

        while (liczba_kas > docelowe)
            zamknij_kase();

    }

    // Zamknięcie wszystkiego
    while (liczba_kas > 0)
        zamknij_kase();

    shmdt(shm);
    printf("[REJESTRACJA] Zamykanie systemu biletowego\n");
    return 0;
}