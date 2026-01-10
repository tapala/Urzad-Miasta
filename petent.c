#include "common.h"


int main() {
    srand(time(NULL));

    // Inicjalizacja handlerów pamięci współdzielonej i kolejki komunikatów
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);

    int semid = semget(ftok(FTOK_PATH, ID_SEM), 2, 0);
    int msg_bilet_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0);
    int msg_urzad_id = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0);

    petent_loop(shmid, semid, msg_bilet_id, msg_urzad_id);

    return 0;
}

void petent_loop(int shmid, int semid, int msg_bilet_id, int msg_urzad_id) {

    // Pamięć współdzielona
    SharedData *shm = (SharedData*)shmat(shmid, NULL, 0);

    // Ustawiamy/losujemy początkowe dane
    pid_t my_pid = getpid();
    int cel = 0;
    int vip = (rand() % 100 < 2);

    // Obsługa wieku
    int wiek = (rand() % 90) + 1;               // Losujemy Wiek
    int wiek_opiekuna = 0;
    int zajmowane_miejsca = 1;

    if (wiek < 18) {                            // Jeśli dziecko to losujemy wiek opiekuna i ustawiamy zajmowane miejsce na 2
        wiek_opiekuna = (rand() % 60) + 18;
        zajmowane_miejsca = 2;
    }

    // Wybieramy gdzie petent ma pójść
    int r = rand() % 100;
    if (r < 60) cel = DEPT_SA;
    else if (r < 70) cel = DEPT_SC;
    else if (r < 80) cel = DEPT_KM;
    else if (r < 90) cel = DEPT_ML;
    else cel = DEPT_PD;

    // Opuszczamy semafor budynku i blokujemy pamięć współdzieloną
    sem_op(semid, SEM_BUDYNEK, -zajmowane_miejsca);
    sem_p(semid, SEM_MUTEX);

    //Zwiększamy liczbę petentów w budynku i patrzymy czy nie przekraczamy limitu departamentu oraz wyciągamy flagę pracy urzędu
    shm->liczba_petentow_w_budynku += zajmowane_miejsca;
    int limit_ok = shm->limity_przyjec[cel] > 0;
    int status = shm->koniec_pracy;

    // Odblokowywujemy pamięć współdzieloną
    sem_v(semid, SEM_MUTEX);

    // Jeśli urząd przestał pracować lub limit został przekroczony i nie jest vipem petent wychodzi z budynku i kończy swój proces
    if (status > 0 || (!limit_ok && !vip)) {
        sem_p(semid, SEM_MUTEX);
        shm->liczba_petentow_w_budynku -= zajmowane_miejsca;
        sem_v(semid, SEM_MUTEX);

        sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca);
        shmdt(shm);
        return;
    }

    // Chcwilowo blokujemy pamięć i zwiększamy liczbę ludzików w kolejce o 1
    sem_p(semid, SEM_MUTEX);
    shm->kolejka_do_biletow += 1;
    sem_v(semid, SEM_MUTEX);

    // Tworzymy komunikat...
    Komunikat msg;
    msg.mtype = 1;
    msg.pid_petenta = my_pid;
    msg.jest_vip = vip;
    msg.wiek = wiek;
    msg.wiek_opiekuna = wiek_opiekuna;

    // ...i wysyłamy do do biletomatu...
    msgsnd(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), 0);

    // ...po czym pobieramy komunikat zwrotny...
    msgrcv(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0);

    // ...a na koniec zmniejszamy piczbę petentów w kolejce o 1
    sem_p(semid, SEM_MUTEX);
    shm->kolejka_do_biletow -= 1;
    sem_v(semid, SEM_MUTEX);


    int zalatwione = 0;
    while (!zalatwione && shm->koniec_pracy != 2) {

        // Wysyłamy petenta do odpowiedniego urzędasa
        msg.mtype = cel;
        msg.typ_sprawy = cel;

        msgsnd(msg_urzad_id, &msg, sizeof(Komunikat) - sizeof(long), 0);

        // Jeśli komunikat się nie powiedzie to przerywamy pętlę
        if (msgrcv(msg_urzad_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1)
            break;

        // Jeśli urzędnik zwróci jedną z dwóch wartości odpowiadającyhc za załatwienie sprawy to ustawiamy flagę załatwione na 1 co przerywa pętlę
        if (msg.typ_sprawy == 0 || msg.typ_sprawy == -1)
            zalatwione = 1;

        // W przeciwnym razie urzędnik SA nas odesłał do innego urzędnika i musimy powtórzyć proces nadpisując cel(wszystko jest robione na referencji do wiadomości)
        else
            cel = msg.typ_sprawy;
    }

    // Petent obsłużony wychodzi z budynku
    sem_p(semid, SEM_MUTEX);
    shm->liczba_petentow_w_budynku -= zajmowane_miejsca;
    sem_v(semid, SEM_MUTEX);

    sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca);

    shmdt(shm);
}