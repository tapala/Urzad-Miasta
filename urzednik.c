#include "common.h"

void start_urzednik();
void signal_handler(int a);
void director_shutdown(int a);
void setitimer_wrapper(suseconds_t a);
void set_queue_id();
void empty_msgqueue();

int typ_wydzialu, shmid, semid, msg_urzad, msg_urzad_back, queue_id, queue_back_id;
int close_flag = 0;
int sa_zamkiete = 0;
volatile int sigflag = 0;

SharedData *shm;
Komunikat msg;
int main(int argc, char **argv) {
    signal(SIGUSR1, director_shutdown);
    signal(SIGRTMIN, signal_handler);
    typ_wydzialu = atoi(argv[1]);
    set_queue_id();

    unsigned int seed = time(NULL) ^ (getpid() << 16);
    srand(seed);

    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0600);
    semid = semget(ftok(FTOK_PATH, ID_SEM), 5, 0600);
    msg_urzad = msgget(ftok(FTOK_PATH, queue_id), 0600);
    msg_urzad_back = msgget(ftok(FTOK_PATH, queue_back_id), 0600);
    shm = (SharedData*)shmat(shmid, NULL, 0);
    start_urzednik();
}

void signal_handler(int sig){
    if(sig == SIGRTMIN){
        sigflag = 1;
        set_queue_id();
        int a1 = 1;
        int a2 = 1;
        while(a1 || a2){
            if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), -3, IPC_NOWAIT) == -1){
                if (errno == ENOMSG) {
                    a1 = 0;
                } 
                else if(errno == EINTR){
                    continue;
                }
                else if(errno){
                    fprintf(stderr,"%s - Wyjebka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                    return;
                }
            }
            if (msgrcv(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), -3, IPC_NOWAIT) == -1){
                if (errno == ENOMSG) {
                    a2 = 0;
                } 
                else if(errno == EINTR){
                    continue;
                }
                else if(errno){
                    fprintf(stderr,"%s - Wyjebka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                    return;
                }
            }
        }
    }
}

void set_queue_id(){
    if (typ_wydzialu == DEPT_SA){
        queue_id = ID_MSG_URZAD_SA;
        queue_back_id = ID_MSG_URZAD_SA_BACK;
    }
    else if(typ_wydzialu == DEPT_SC){
        queue_id = ID_MSG_URZAD_SC;
        queue_back_id = ID_MSG_URZAD_SC_BACK;
    }
    else if(typ_wydzialu == DEPT_KM){
        queue_id = ID_MSG_URZAD_KM;
        queue_back_id = ID_MSG_URZAD_KM_BACK;
    }
    else if(typ_wydzialu == DEPT_ML){
        queue_id = ID_MSG_URZAD_ML;
        queue_back_id = ID_MSG_URZAD_ML_BACK;
    }
    else if(typ_wydzialu == DEPT_PD){
        queue_id = ID_MSG_URZAD_PD;
        queue_back_id = ID_MSG_URZAD_PD_BACK;
    }
    else if(typ_wydzialu == DEPT_KASA){
        queue_id = ID_MSG_URZAD_KASA;
        queue_back_id = ID_MSG_URZAD_KASA_BACK;
    }
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
        // Sprawdzanie stanu pracy urzędu
        while(sem_p(semid, SEM_MUTEX));
        int stan = shm->koniec_pracy;
        while(sem_v(semid, SEM_MUTEX));

        // Ewakuacja natychmiastowa
        if (stan > 0){
            while(sem_p(semid, SEM_MUTEX));
            if (!(shm->sa_zamkiete)){
                shm->sa_zamkiete = 1;
            }
            else{
                sa_zamkiete = 1;
            }
            while(sem_v(semid, SEM_MUTEX));
            break;
        }   

        if(msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), -3, 0) == -1){
            if(errno == EINTR){
                if(!sigflag)
                    goto signal_skip;
                continue;
            }
            else{
                perror("Wyjebka message queue");
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
                repeat_150:
                if(msgsnd(msg_bilet, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
                    if(errno == EINTR){
                        if(!sigflag)
                            goto repeat_150;
                        continue;
                    }
                    else{
                        fprintf(stderr,"%s - Wyjebka na msgsnd - SA => Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                    }
                    
                }

                repeat_161:
                if(msgrcv(msg_bilet_back, &msg, sizeof(Komunikat) - sizeof(long), msg.pid_petenta, 0) == -1){
                    if(errno == EINTR){
                        if(!sigflag)
                            goto repeat_161;
                        continue;
                    }
                    else{
                        fprintf(stderr,"%s - Wyjebka na msgrcv - Rejestracja => SA - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                    }
                    
                }

                if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
                    sprintf(log_buf, "[URZĘDNIK %d] Brak miejsc u urzednika %d dla Petenta %d\n", typ_wydzialu, cel, msg.pid_petenta);
                    log_to_file(log_buf, semid);
                }
                else{
                    printf("[URZĘDNIK %d] Przekierowano petenta %d do %d\n", typ_wydzialu, msg.pid_petenta, cel);
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
        if(typ_wydzialu == DEPT_KASA){
            msg.kasa_odwiedzona = 1;
            msg.typ_sprawy = msg.cel_po_kasie; 
        }

        repeat_197:
        if(msgsnd(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            if(errno == EINTR){
                if(!sigflag)
                    goto repeat_197;
            }
            else{
            fprintf(stderr,"%s - Wyjebka na msgsnd do klienta - %d ||| %d \n", strerror(errno), typ_wydzialu,__LINE__);                        
            }

        }
    }

    signal_skip:

    if(typ_wydzialu != DEPT_SA || sa_zamkiete){
        empty_msgqueue();
        msgctl(msg_urzad, IPC_RMID, NULL);
        msgctl(msg_urzad_back, IPC_RMID, NULL);
    }

    printf("\033[33m[URZEDNIK %d] Koniec - obsluzeni: %d\033[m\n",typ_wydzialu, obsluzeni);
    // Odłączenie pamięci współdzielonej po zakończeniu loopa - ewakuacja
    shmdt(shm);
    while(sem_v(semid, SEM_LOCK_REGISTER));
}

void director_shutdown(int a){
    if (typ_wydzialu == DEPT_SA){
        sa_zamkiete = shm->sa_zamkiete;
        while(sem_p(semid, SEM_MUTEX));
        if(sa_zamkiete){
            shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
            shm->limity_przyjec[typ_wydzialu] = 0;
        }
        else{
            shm->sa_zamkiete = 1;
        }
        while(sem_v(semid, SEM_MUTEX));
        close_flag = 1;
    }
    else{
        while(sem_p(semid, SEM_MUTEX));
        shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
        shm->limity_przyjec[typ_wydzialu] = 0;
        while(sem_v(semid, SEM_MUTEX));
        close_flag = 1;
    }
}

void empty_msgqueue(){
    while(1){
        if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), -3, IPC_NOWAIT) == -1){
            if (errno == ENOMSG) {
                break;
            } 
            else if(errno == EINTR){
                continue;
            }
            else if(errno){
                fprintf(stderr,"%s - Wyjebka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                return;
            }
        }
        else{
            msg.mtype = msg.pid_petenta;
            msg.typ_sprawy = KONIEC_OBSLUGI;

            repeat_262:
            if(msgsnd(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
                if(errno == EINTR){
                    goto repeat_262;
                }
                else{
                fprintf(stderr,"%s - Wyjebka na msgsnd przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                return;                    
                }

            } 
        }
    }
}