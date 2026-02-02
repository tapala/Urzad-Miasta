#include "common.h"

void start_urzednik();
void signal_handler(int a);
void director_shutdown();
void setitimer_wrapper(suseconds_t a);
void set_queue_id();
void empty_msgqueue();
void czysciciel();

int typ_wydzialu, monitor_offset,shmid, semid, msg_urzad, msg_urzad_back, queue_id, queue_back_id, obsluzeni;
int close_flag = 0;
int sa_zamkiete = 0;
volatile int sigflag = 0;

int mshmid;
MonitorData *mshm;

SharedData *shm;
Komunikat msg;
int main(int argc, char **argv) {

    if(signal(SIGUSR1, signal_handler) == SIG_ERR || signal(SIGRTMIN, signal_handler) == SIG_ERR || signal(SIGUSR2, signal_handler) == SIG_ERR){
        perror("signal urzednik");
        exit(1);
    }

    typ_wydzialu = atoi(argv[1]);
    monitor_offset = atoi(argv[2]);
    set_queue_id();
    unsigned int seed = time(NULL) ^ (getpid() << 16);
    srand(seed);
    init_signals();

    semid = semget(ftok_handler(FTOK_PATH, ID_SEM), 6, 0);
    if(semid == -1){
        perror("semget urzednik");
    }

    msg_urzad = msgget(ftok_handler(FTOK_PATH, queue_id), 0);
    if(msg_urzad == -1){
        perror("msgget urzednik msg_urzad");
    }
    msg_urzad_back = msgget(ftok_handler(FTOK_PATH, queue_back_id), 0);
    if(msg_urzad_back == -1){
        perror("msgget urzednik msg_urzad_back");
    }

    shmid = shmget(ftok_handler(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    mshmid = shmget(ftok_handler(FTOK_PATH, ID_SHM_MONITOR), sizeof(MonitorData), 0);
    if(shmid == -1 || mshmid == -1){
        perror("shmget urzednik");
    }

    shm = (SharedData*)shmat(shmid, NULL, 0);
    if(shm == (void*)-1){
        perror("shmat biletomat shm");
        exit(1);
    }
    mshm = (MonitorData*)shmat(mshmid, NULL, 0);
    if(mshm == (void*)-1){
        perror("shmat biletomat mshm");
        exit(1);
    }

    start_urzednik();
    
}

void signal_handler(int sig){
    if(sig == SIGRTMIN){
        czysciciel();
    }
    else if(sig == SIGUSR1){
        director_shutdown();
    }
}

void czysciciel(){
    sigflag = 1;
    set_queue_id();
    int a1 = 1;
    int a2 = 1;
    while(a1 || a2){
        if(a1){
            if (msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), -3, IPC_NOWAIT) == -1){
                if (errno == ENOMSG) {
                    a1 = 0;
                } 
                else if(errno == EINTR){
                    continue;
                }
                else if(errno){
                    fprintf(stderr,"%s - Wywrotka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                    exit(1);
                }
            }
        }
        if(a2){
            if (msgrcv(msg_urzad_back, &msg, sizeof(Komunikat) - sizeof(long), -3, IPC_NOWAIT) == -1){
                if (errno == ENOMSG) {
                    a2 = 0;
                } 
                else if(errno == EINTR){
                    continue;
                }
                else if(errno){
                    fprintf(stderr,"%s - Wywrotka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                    exit(1);
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
    int msg_bilet = msgget(ftok_handler(FTOK_PATH, ID_MSG_BILET), 0);
    int msg_bilet_back = msgget(ftok_handler(FTOK_PATH, ID_MSG_BILET_BACK), 0);
    if(msg_bilet == -1 || msg_bilet_back == -1){
        perror("msget biletomat bilet");
        exit(1);

    }
    // Podstawowy syf
    obsluzeni = 0;
    char log_buf[256];

    printf("\033[33m[URZĘDNIK %d] Rozpoczyna pracę.\033[m\n", typ_wydzialu);

    while (!close_flag)
    {
        // Sprawdzanie stanu pracy urzędu
        sem_p_mutex(semid);
        int stan = shm->koniec_pracy;
        sem_v_mutex(semid);
        
        if(msgrcv(msg_urzad, &msg, sizeof(Komunikat) - sizeof(long), -3, 0) == -1){
            if(errno == EINTR){
                if(!sigflag)
                    goto signal_skip;
                continue;
            }
            else{
                perror("Wywrotka message queue");
            }
        }

        // Komunikaty informacyjne
        if(typ_wydzialu != DEPT_KASA){
            if (msg.jest_vip){
                printf("\033[35m[URZĘDNIK %d] OBSŁUGA VIP PID %d\033[m\n", typ_wydzialu, msg.pid_petenta);
            }
            else if (msg.wiek < 18){
                printf("\033[38;5;207m[URZĘDNIK %d] Obsługa dziecka z opiekunem(PID %d)\033[m\n", typ_wydzialu, msg.pid_petenta);
            }
            else{
                printf("\033[32m[URZĘDNIK %d] Obsługa dorosłego(PID %d)\033[m\n", typ_wydzialu, msg.pid_petenta);
            }
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
                        fprintf(stderr,"%s - Wywrotka na msgsnd - SA => Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
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
                        fprintf(stderr,"%s - Wywrotka na msgrcv - Rejestracja => SA - %s => %d \n", strerror(errno), __FILE__,__LINE__);
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
            while(sem_p(semid, SEM_MONITOR_MUTEX));
            if(msg.jest_vip){
                mshm->vipy[typ_wydzialu - monitor_offset]++;
            }
            obsluzeni++;
            mshm->obslozeni[typ_wydzialu - monitor_offset]++;
            while(sem_v(semid, SEM_MONITOR_MUTEX));
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
            fprintf(stderr,"%s - Wywrotka na msgsnd do klienta - %d ||| %d \n", strerror(errno), typ_wydzialu,__LINE__);                        
            }

        }
    }

    signal_skip:

    if(typ_wydzialu != DEPT_SA || sa_zamkiete){
        empty_msgqueue();
        msgctl_IPC_RMID_handler(msg_urzad);
        msgctl_IPC_RMID_handler(msg_urzad_back);
    }

    sprintf(log_buf, "[URZEDNIK %d] Koniec - obsluzeni: %d\n", typ_wydzialu, obsluzeni);
    log_to_file(log_buf, semid);
    printf("\033[33m[URZEDNIK %d] Koniec - obsluzeni: %d\033[m\n",typ_wydzialu, obsluzeni);
    // Odłączenie pamięci współdzielonej po zakończeniu loopa - ewakuacja
    
    if(shmdt(shm) == -1){
        perror("shmdt");
    }
    while(sem_v(semid, SEM_LOCK_REGISTER));
}

void director_shutdown(){
    if (typ_wydzialu == DEPT_SA){
        sem_p_mutex(semid);
        sa_zamkiete = shm->sa_zamkiete;
        if(sa_zamkiete){
            shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
            shm->limity_przyjec[typ_wydzialu] = 0;
        }
        else{
            shm->sa_zamkiete = 1;
        }
        sem_v_mutex(semid);
        close_flag = 1;
    }
    else{
        sem_p_mutex(semid);
        shm->limity_przyjec_sum -= shm->limity_przyjec[typ_wydzialu];
        shm->limity_przyjec[typ_wydzialu] = 0;
        sem_v_mutex(semid);
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
                fprintf(stderr,"%s - Wywrotka na msgrcv przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
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
                fprintf(stderr,"%s - Wywrotka na msgsnd przy końcu pracy urzednika- %d \n", strerror(errno), __LINE__);
                return;                    
                }

            } 
        }
    }
}