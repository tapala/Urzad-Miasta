#include "common.h"

void start_urzednik(int typ_wydzialu, int limit_dzienny);

int main(int argc, char **argv) {
    int typ_wydzialu = atoi(argv[1]);
    int limit = atoi(argv[2]);
    start_urzednik(typ_wydzialu, limit);
}

void start_urzednik(int typ_wydzialu, int limit_dzienny) {

    // Inicjalizacja pamięci, semaforów, kolejki komunikatów
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0600);
    SharedData *shm = (SharedData*)shmat(shmid, NULL, 0);

    int semid = semget(ftok(FTOK_PATH, ID_SEM), 4, 0600);
    int msg_bilet = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0600);
    int msg_bilet_back = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0600);
    int msg_urzad = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0600);
    int msg_urzad_back = msgget(ftok(FTOK_PATH, ID_MSG_URZAD_BACK), 0600);

    // Podstawowy syf
    int obsluzeni = 0;
    Komunikat msg;
    char log_buf[256];

    printf("[URZĘDNIK %d] Rozpoczyna pracę.\n", typ_wydzialu);

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
                else if(errno != ENOMSG){
                    perror("Wyjebka message queue");
                }

                continue;  // Nic do roboty więc czeka dalej
            }
        }



        // Komunikaty informacyjne

        if (msg.jest_vip)
            printf("[URZĘDNIK %d] OBSŁUGA VIP PID %d\n", typ_wydzialu, msg.pid_petenta);
        else if (msg.wiek < 18)
            printf("[URZĘDNIK %d] Obsługa dziecka z opiekunem(PID %d)\n", typ_wydzialu, msg.pid_petenta);
        else
            printf("[URZĘDNIK %d] Obsługa dorosłego(PID %d)\n", typ_wydzialu, msg.pid_petenta);

        // Symulacja obsługi czasu
        //usleep(1000);


        // Przekierowania
        int przekierowanie = 0;
        int cel = 0;

        // Jeśli urząd funkcjonuje
        if (stan == 0) {

            // Z SA losowo do innych
            if (typ_wydzialu == DEPT_SA && !msg.odeslany_z_sa && (rand() % 100 < 40))
            {
                przekierowanie = 1;
                cel = (rand() % 4) + 2;  // SC, KM, ML, PD
            }
            // Z innych do kasy
            else if (typ_wydzialu != DEPT_SA && typ_wydzialu != DEPT_KASA && (rand() % 100 < 10))
            {
                przekierowanie = 1;
                cel = DEPT_KASA;
            }
        }

        // Odpowiadamy petentowy
        msg.mtype = msg.pid_petenta;

        if (przekierowanie) {
            // Przy przekierowaniu do wiadomości dodajemy cel przekierowania
            msg.typ_sprawy = cel;

            
            if (typ_wydzialu == DEPT_SA)
                msg.odeslany_z_sa = 1;

            if(cel != DEPT_KASA){
                msg.mtype = 1;
                if(msgsnd(msg_bilet, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
                    fprintf(stderr,"%s - Wyjebka na msgsnd - SA => Kasa - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                }

                if(msgrcv(msg_bilet_back, &msg, sizeof(Komunikat) - sizeof(long), msg.pid_petenta, 0) == -1){
                    fprintf(stderr,"%s - Wyjebka na msgrcv - Kasa => SA - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                }

                if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
                    // Brak miejsc - wpis do raportu dziennego
                }
            }
            
            sprintf(log_buf, "RAPORT: WYDZIAŁ %d przekierował PID %d do %d\n", typ_wydzialu, msg.pid_petenta, cel);

            log_to_file(log_buf);
        }
        else {
            // Sprawa załatwiona
            msg.typ_sprawy = 0;
        }

        obsluzeni++;
        // Odsyłamy wiadomość
        if(msgsnd(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            fprintf(stderr,"%s - Wyjebka na msgqueue - %d \n", strerror(errno), __LINE__);
        }
    }
    printf("[URZEDNIK %d] Koniec - obsluzeni: %d\n",typ_wydzialu, obsluzeni);
    // Odłączenie pamięci współdzielonej po zakończeniu loopa - ewakuacja
    shmdt(shm);
    
}