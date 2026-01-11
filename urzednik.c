#include "common.h"

void start_urzednik(int typ_wydzialu, int limit_dzienny) {

    // Inicjalizacja pamięci, semaforów, kolejki komunikatów
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0666);
    SharedData *shm = (SharedData*)shmat(shmid, NULL, 0);

    int semid = semget(ftok(FTOK_PATH, ID_SEM), 2, 0666);
    int msg_urzad = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0666);

    // Podstawowy syf
    int obsluzeni = 0;
    Komunikat msg;
    char log_buf[256];

    printf("[URZĘDNIK] Rozpoczyna pracę.");

    while (1)
    {
        // Sprawdzanie stanu pracy urzędu
        sem_p(semid, SEM_MUTEX);
        int stan = shm->koniec_pracy;
        sem_v(semid, SEM_MUTEX);

        // Ewakuacja natychmiastowa
        if (stan == 2)
            break;   


        //  PRIORYTET VIP w kolejce do urzędnika
        long vip_type    = typ_wydzialu * 10 + 1;
        long normal_type = typ_wydzialu * 10 + 2;

        // Najpierw spróbuj odebrać VIP
        if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), vip_type, IPC_NOWAIT) == -1)
        {
            // Brak VIP – odbieramy zwykłych
            int flag = (stan == 1) ? IPC_NOWAIT : 0;

            if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), normal_type, flag) == -1)
            {
                if (errno == ENOMSG && stan == 1)
                    break; // Koniec dnia + pusta kolejka

                continue;  // Nic do roboty więc czeka dalej
            }
        }

        if (obsluzeni >= limit_dzienny && typ_wydzialu != DEPT_KASA) {

            // Logujemy do pliku przekroczony limit dzienny
            sprintf(log_buf, "RAPORT: WYDZIAŁ %d odrzucił PID %d (limit wyczerpany)\n", typ_wydzialu, msg.pid_petenta);
            log_to_file(log_buf);

            msg.mtype = msg.pid_petenta;
            msg.typ_sprawy = -1;

            msgsnd(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), 0);

            continue;
        }


        // Komunikaty informacyjne

        if (msg.jest_vip)
            printf("[URZĘDNIK] OBSŁUGA VIP PID %d\n", msg.pid_petenta);
        else if (msg.wiek < 18)
            printf("[URZĘDNIK] Obsługa dziecka z opiekunem(PID %d)\n", msg.pid_petenta);
        else
            printf("[URZĘDNIK] Obsługa dorosłego(PID %d)\n", typ_wydzialu, msg.wiek, msg.pid_petenta);

        usleep(rand() % 150000 + 50000);

        
    }


    
}