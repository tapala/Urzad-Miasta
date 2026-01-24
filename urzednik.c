#include "common.h"

void start_urzednik();
void signal_handler(int a);
void setitimer_wrapper(suseconds_t a);
void queue_cleanup();

int typ_wydzialu, shmid, semid, msg_urzad, msg_urzad_back;
int close_flag = 0;
int sa_zamkiete = 0;

SharedData *shm;
Komunikat msg;
int main(int argc, char **argv) {
    signal(SIGALRM,signal_handler);
    typ_wydzialu = atoi(argv[1]);
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0600);
    semid = semget(ftok(FTOK_PATH, ID_SEM), 4, 0600);
    msg_urzad = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0600);
    msg_urzad_back = msgget(ftok(FTOK_PATH, ID_MSG_URZAD_BACK), 0600);
    shm = (SharedData*)shmat(shmid, NULL, 0);
    start_urzednik();
}

void signal_handler(int a){

}

void start_urzednik() {

    // Inicjalizacja pamięci, semaforów, kolejki komunikatów
    int msg_bilet = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0600);
    int msg_bilet_back = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0600);

    // Podstawowy syf
    int obsluzeni = 0;
    char log_buf[256];

    printf("[URZĘDNIK %d] Rozpoczyna pracę.\n", typ_wydzialu);

    while (!close_flag)
    {
        usleep(1000000);
        // Sprawdzanie stanu pracy urzędu
        sem_p(semid, SEM_MUTEX);
        int stan = shm->koniec_pracy;
        sem_v(semid, SEM_MUTEX);

        // Ewakuacja natychmiastowa
        if (stan == 1 || stan == 2){
            sem_p(semid, SEM_MUTEX);
            if (!(shm->sa_zamkiete)){
                shm->sa_zamkiete = 1;
            }
            else{
                sa_zamkiete = 1;
            }
            sem_v(semid, SEM_MUTEX);
            break;
        }   

        //  PRIORYTET VIP w kolejce do urzędnika
        long kasa_type = typ_wydzialu * 10 + 1;
        long vip_type = typ_wydzialu * 10 + 2;
        long normal_type = typ_wydzialu * 10 + 3;

        // Najpierw bierzemy wracających z kasy
        if(msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), kasa_type, IPC_NOWAIT) == -1){
            // Potem spróbuj odebrać VIP
            if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), vip_type, IPC_NOWAIT) == -1)
            {
                // Brak VIP – odbieramy zwykłych
                int flag = (stan == 1) ? IPC_NOWAIT : 0;
                alarm(1);
                int rc = msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), normal_type, flag);
                alarm(0);
                if (rc == -1)
                {
                    if(errno != ENOMSG && errno != EINTR){
                        perror("Wyjebka message queue");
                    }
                    else if (stan == 1 || stan == 2)
                        break; // Koniec dnia + pusta kolejka
                    continue;  // Nic do roboty więc czeka dalej
                }
            }
        }

        // Komunikaty informacyjne
        if(typ_wydzialu != DEPT_KASA){
            if (msg.jest_vip)
                printf("\033[35m[URZĘDNIK %d] OBSŁUGA VIP PID %d\033[m\n", typ_wydzialu, msg.pid_petenta);
            else if (msg.wiek < 18)
                printf("\033[38;5;207m[URZĘDNIK %d] Obsługa dziecka z opiekunem(PID %d)\033[m\n", typ_wydzialu, msg.pid_petenta);
            else
                printf("\033[32m[URZĘDNIK %d] Obsługa dorosłego(PID %d)\033[m\n", typ_wydzialu, msg.pid_petenta);
        }

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
                msg.cel_po_kasie = typ_wydzialu;
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
                    fprintf(stderr,"%s - Wyjebka na msgsnd - SA => Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                }

                if(msgrcv(msg_bilet_back, &msg, sizeof(Komunikat) - sizeof(long), msg.pid_petenta, 0) == -1){
                    fprintf(stderr,"%s - Wyjebka na msgrcv - Rejestracja => SA - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                }

                if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
                    sprintf(log_buf, "[URZĘDNIK %d] Brak miejsc u urzednika %d dla Petenta %d\n", typ_wydzialu, cel, msg.pid_petenta);
                    log_to_file(log_buf);
                }
                else{
                    printf(log_buf, "[URZĘDNIK %d] Przekierowano petenta %d do %d\n", typ_wydzialu, msg.pid_petenta, cel);
                    //log_to_file(log_buf);
                    fflush(stdout);
                }
            }
            
        }
        else {
            // Sprawa załatwiona
            msg.typ_sprawy = 0;
        }
        if(cel != DEPT_KASA){
            obsluzeni++;
        }

        // Odsyłamy wiadomość
        if(typ_wydzialu == DEPT_KASA || typ_wydzialu == DEPT_KASA){
            msg.kasa_odwiedzona = 1;
            msg.typ_sprawy = msg.cel_po_kasie; 
        }
        if(msgsnd(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            fprintf(stderr,"%s - Wyjebka na msgqueue - %d \n", strerror(errno), __LINE__);
        }
    }

    if(typ_wydzialu != DEPT_SA || sa_zamkiete){
        queue_cleanup();
    }

    printf("\033[33m[URZEDNIK %d] Koniec - obsluzeni: %d\033[m\n",typ_wydzialu, obsluzeni);
    // Odłączenie pamięci współdzielonej po zakończeniu loopa - ewakuacja
    shmdt(shm);
    
}

void director_shutdown(){
    if (typ_wydzialu == DEPT_SA){
        sa_zamkiete = shm->sa_zamkiete;
        sem_p(semid, SEM_MUTEX);
        if(sa_zamkiete){
            shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
            shm->limity_przyjec[typ_wydzialu] = 0;
        }
        else{
            shm->sa_zamkiete = 1;
        }
        sem_v(semid, SEM_MUTEX);
        close_flag = 1;
    }
    else{
        sem_p(semid, SEM_MUTEX);
        shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
        shm->limity_przyjec[typ_wydzialu] = 0;
        sem_v(semid, SEM_MUTEX);
        close_flag = 1;
    }
}

void empty_msgqueue(long type){
    while(1){
        if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), type, IPC_NOWAIT) == -1){
            if (errno == ENOMSG) {
                break;
            } 
            else {
                fprintf(stderr,"%s - Wyjebka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                return;
            }
        }
        else{
            msg.mtype = msg.pid_petenta;
            msg.typ_sprawy = KONIEC_OBSLUGI;
            if(msgsnd(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
                fprintf(stderr,"%s - Wyjebka na msgsnd przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                return;
            } 
        }
    }
}

void queue_cleanup(){
    long kasa_type = typ_wydzialu * 10 + 1;
    long vip_type = typ_wydzialu * 10 + 2;
    long normal_type = typ_wydzialu * 10 + 3;
    int cleanup_flag;
    do{
        empty_msgqueue(kasa_type);
        empty_msgqueue(vip_type);
        empty_msgqueue(normal_type);
        sem_p(semid, SEM_MUTEX);
        cleanup_flag = shm->brak_petentow;
        sem_v(semid, SEM_MUTEX);
    }while(!cleanup_flag);
}