#include "common.h"
#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

atomic_int bilet_gotowy = 0;
int shmid, semid,  msg_bilet_id, msg_bilet_back_id, msg_urzad_id, msg_urzad_back_id, cel, queue_id, queue_back_id;
int zajmowane_miejsca = 1;
pid_t my_pid;
char log_buf[256];

SharedData *shm;

Komunikat msg;

void kys();
void petent_loop();
void* opiekun_thread(void *arg);
void set_queue_id();
void atexit_action(void);

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cel>\n", argv[0]);
        exit(1);
    }

    cel = atoi(argv[1]);

    if (cel <= 0) {
        fprintf(stderr, "Invalid cel value: %s\n", argv[1]);
        exit(1);
    }

    if(signal(SIGRTMIN, kys) == SIG_ERR){
        perror("signal petent");
    }

    if(atexit(atexit_action)){
        perror("atexit petent");
        while(sem_v(semid, SEM_PETENCI));
        exit(1);
    }

    init_signals();

    unsigned int seed = time(NULL) ^ (getpid() << 16);
    srand(seed);
    set_queue_id();
    // Inicjalizacja handlerów pamięci współdzielonej i kolejki komunikatów
    shmid = shmget(ftok_handler(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    if(shmid == -1){
        perror("shmget petent");
        exit(1);
    }

    semid = semget(ftok_handler(FTOK_PATH, ID_SEM), 6, 0);
    if(semid == -1){
        perror("semget petent");
        exit(1);
    }

    init_signals();

    msg_bilet_id = msgget(ftok_handler(FTOK_PATH, ID_MSG_BILET), 0);
    msg_bilet_back_id = msgget(ftok_handler(FTOK_PATH, ID_MSG_BILET_BACK), 0);
    msg_urzad_id = msgget(ftok_handler(FTOK_PATH, queue_id), 0);
    msg_urzad_back_id = msgget(ftok_handler(FTOK_PATH, queue_back_id), 0);

    if(msg_bilet_id == -1){
        fprintf(stderr,"%s - msgget bilet; cel = %d - %s => %d \n", strerror(errno), cel,__FILE__,__LINE__);
        exit(1);
    }
    if(msg_bilet_back_id == -1){
        fprintf(stderr,"%s - msgget bilet back; cel = %d - %s => %d \n", strerror(errno), cel,__FILE__,__LINE__);
        exit(1);
    }
    if(msg_urzad_id == -1){
        fprintf(stderr,"%s - msgget urzad; cel = %d - %s => %d \n", strerror(errno), cel,__FILE__,__LINE__);
        exit(1);
    }
    if(msg_urzad_back_id == -1){
        fprintf(stderr,"%s - msgget urzad back; cel = %d - %s => %d \n", strerror(errno), cel,__FILE__,__LINE__);
        exit(1);
    }

    petent_loop();

    return 0;
}

void* opiekun_thread(void *arg)
{
    (void)arg;
    int ret;
    // Wejście do kolejki biletowej
    sem_p_mutex(semid);
    shm->kolejka_do_biletow++;
    sem_v_mutex(semid);

    // Wysłanie żądania biletu
    if(msgsnd(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
        if(errno != EINTR)
            fprintf(stderr,"%s - Wyjebka na msgsnd - Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
    }

    // Odebranie biletu adresowanego do PID petenta
    if(msgrcv(msg_bilet_back_id, &msg, sizeof(Komunikat) - sizeof(long), msg.pid_petenta, 0) == -1){
        if(errno != EINTR)
            fprintf(stderr,"%s - Wyjebka na msgrcv - Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
    }

    if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
        printf("\033[41m[PETENT %d] Brak miejsc u urzednika %d. Z zalu popelniam sudoku\033[m\n", my_pid, cel);
        fflush(stdout);
        sem_p_mutex(semid);
        shm->kolejka_do_biletow--;
        sem_v_mutex(semid);
        kys();
    }

    // Informacja dla dziecka - wręczenie biletu
    ret = pthread_mutex_lock(&mtx);
    if (ret) {
        perror("pthread_mutex_lock");
        return NULL;
    }
    bilet_gotowy = 1;
    ret = pthread_cond_signal(&cond);
    if(ret) {
        fprintf(stderr, "pthread_cond_signal - wywrotka: %s\n", strerror(ret));
    }
    ret = pthread_mutex_unlock(&mtx);
    if(ret) {
        fprintf(stderr, "pthread_mutex_unlock - wywrotka: %s\n", strerror(ret));
    }

    return NULL;
}

void petent_loop() {

    // Pamięć współdzielona
    shm = (SharedData*)shmat(shmid, NULL, 0);
    if (shm == (void*)-1) {
        perror("shmat petent");
        exit(1);
    }

    // Ustawiamy/losujemy początkowe dane
    my_pid = getpid();
    int vip = (rand() % 100 < 2);

    // Obsługa wieku
    int wiek;

    if(vip)
        wiek = (rand() % 73) + 18;
    else
        wiek = (rand() % 90) + 1;               // Losujemy Wiek

    int wiek_opiekuna = 0;

    if (wiek < 18) {                            // Jeśli dziecko to losujemy wiek opiekuna i ustawiamy zajmowane miejsce na 2
        wiek_opiekuna = (rand() % 60) + 18;
        zajmowane_miejsca = 2;
    }

    sem_p_mutex(semid);
    if(!shm->limity_przyjec_sum){
        sem_v_mutex(semid);
        printf("\033[44m\033[33m[PETENT %d] Brak miejsc u urzedników. Z zalu popelniam sudoku\033[m\n", my_pid);
        fflush(stdout);
        shmdt(shm);
        exit(1);
    }
    sem_v_mutex(semid);

    sem_p_mutex(semid);
    int status = shm->koniec_pracy;

    sem_v_mutex(semid);

    if (status > 0) {
        shmdt(shm);
        exit(1);
    }
    else{
        sem_op(semid, SEM_BUDYNEK, -zajmowane_miejsca, 0);
    }

    // Tworzymy komunikat...
    msg.mtype = 1;
    msg.pid_petenta = my_pid;
    msg.typ_sprawy = cel;
    msg.jest_vip = vip;
    msg.wiek = wiek;
    msg.wiek_opiekuna = wiek_opiekuna;
    msg.cel_po_kasie = 0;
    msg.kasa_odwiedzona = 0;
    msg.odeslany_z_sa = 0;
    msg.nr_biletu = -1;

    // Jeśli dziecko -> odpal wątek opiekuna
    if (wiek < 18) {

        int ret;
        pthread_t th;
        ret = pthread_create(&th, NULL, opiekun_thread, NULL);
        if(ret){
            fprintf(stderr, "pthread_create - wywrotka: %s\n", strerror(ret));
            kys();
        }

        // Dziecko czeka na rodzica z biletem
        ret = pthread_mutex_lock(&mtx);
        if(ret){
            fprintf(stderr, "pthread_mutex_lock - wywrotka: %s\n", strerror(ret));
            kys();
        }

        while (!bilet_gotowy){
            ret = pthread_cond_wait(&cond, &mtx);
            if(ret){
                fprintf(stderr, "pthread_cond_wait - wywrotka: %s\n", strerror(ret));
                pthread_mutex_unlock(&mtx);
                kys();
            }
        }

        ret = pthread_mutex_unlock(&mtx);
        if(ret){
            fprintf(stderr, "pthread_mutex_unlock - wywrotka: %s\n", strerror(ret));
        }

        ret = pthread_join(th, NULL);
        if(ret){
            fprintf(stderr, "pthread_join - wywrotka: %s\n", strerror(ret));
            kys();
        }
    }
    // Dorosły działa jak wcześniej
    else {
        
        sem_p_mutex(semid);
        shm->kolejka_do_biletow++;
        sem_v_mutex(semid);

        // Komunikat wysyłamy do do biletomatu...
        if(msgsnd(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            if(errno != EINTR){
                fprintf(stderr,"%s -- %d -- Wyjebka na msgsend - Rejestracja - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
                kys();
            }
        }

        // ...po czym pobieramy komunikat zwrotny...
        if(msgrcv(msg_bilet_back_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1){
            if(errno != EINTR){
                fprintf(stderr,"%s -- %d -- Wyjebka na msgrcv - Rejestracja - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
                kys();
            }
        }

        if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
            sem_p_mutex(semid);
            shm->kolejka_do_biletow--;
            sem_v_mutex(semid);
            printf("\033[41m[PETENT %d] Brak miejsc u urzednika %d. Z zalu popelniam sudoku\033[m\n", my_pid, cel);
            fflush(stdout);
            kys();
        }

    }

    // ...a na koniec zmniejszamy piczbę petentów w kolejce o 1
    sem_p_mutex(semid);
    shm->kolejka_do_biletow--;
    sem_v_mutex(semid);

    while (1) {

        sem_p_mutex(semid);
        int status = shm->koniec_pracy;
        sem_v_mutex(semid);

        if (status > 0) break;

        // Wysyłamy petenta do odpowiedniego urzędasa
        if(msg.kasa_odwiedzona){
            msg.mtype = 1;   // Kolejka po powrocie z kasy
        }
        else if (vip) {
            msg.mtype = 2;   // Kolejka VIP
        } 
        else {
            msg.mtype = 3;   // Zwykła kolejka
        }

        msg.typ_sprawy = cel;
        

        if(msgsnd(msg_urzad_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            if(errno == EIDRM || errno == EINVAL){
                printf("\033[42m\033[37m[PETENT %d] Urzędnik się zamknął zanim wszedłem do jego kolejki \033[m\n", my_pid);
                break;
            }
            else if(errno != EINTR){
                fprintf(stderr,"%s -- %d -- Wyjebka na msgsend - Urzednik - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
                break;
            }
        }

        // Jeśli komunikat się nie powiedzie to przerywamy pętlę
        if (msgrcv(msg_urzad_back_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1){
            if(errno != EINTR){
                fprintf(stderr,"%s -- %d -- Wyjebka na msgrcv - Urzednik - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
                break;
            }
        }
        // Jeśli urzędnik zwróci wartość odpowiadającą załatwienie sprawy to przerywamy pętlę
        if (msg.typ_sprawy == 0)
            break;

        else if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
            printf("\033[41m\033[30m[PETENT %d] SA nie mogło mnie przekierowac. Pora umierać      \033[m\n", my_pid);
            break;
        }
        else if(msg.typ_sprawy == KONIEC_OBSLUGI){
            sleep(CZAS_PO_ZAMKNIECIU);
            sprintf(log_buf, "[PETENT %d - NR BILETU %d] Jestem sfrustrowany - nie wejde do %d\n", my_pid, msg.nr_biletu, cel);
            log_to_file(log_buf, semid);
            printf("\033[46m\033[34m[PETENT %d] Jestem Sfrustrowany                       \033[m\n", my_pid);
            fflush(stdout);
            break;
        }
        // W przeciwnym razie urzędnik SA nas odesłał do innego urzędnika i musimy powtórzyć proces nadpisując cel(wszystko jest robione na referencji do wiadomości)
        else{
            cel = msg.typ_sprawy;
            set_queue_id();
            msg_urzad_id = msgget(ftok_handler(FTOK_PATH, queue_id), 0);
            msg_urzad_back_id = msgget(ftok_handler(FTOK_PATH, queue_back_id), 0);
            if(msg_bilet_id == -1){
                fprintf(stderr,"%s - msgget zwrotka - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                exit(1);
            }
            if(msg_urzad_back_id == -1){
                fprintf(stderr,"%s - msgget zwrotka back - %s => %d \n", strerror(errno), __FILE__,__LINE__);
                exit(1);
            }
        }
    }

    // Petent obsłużony wychodzi z budynku

    while(sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca, 0));

    if(shmdt(shm) == -1){
        perror("shmdt");
    }
    exit(0);
}

void kys(){
    while(sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca, 0));
    if(shmdt(shm) == -1){
        perror("shmdt");
    }
    exit(0);
}

void set_queue_id(){
    if (cel == DEPT_SA){
        queue_id = ID_MSG_URZAD_SA;
        queue_back_id = ID_MSG_URZAD_SA_BACK;
    }
    else if(cel == DEPT_SC){
        queue_id = ID_MSG_URZAD_SC;
        queue_back_id = ID_MSG_URZAD_SC_BACK;
    }
    else if(cel == DEPT_KM){
        queue_id = ID_MSG_URZAD_KM;
        queue_back_id = ID_MSG_URZAD_KM_BACK;
    }
    else if(cel == DEPT_ML){
        queue_id = ID_MSG_URZAD_ML;
        queue_back_id = ID_MSG_URZAD_ML_BACK;
    }
    else if(cel == DEPT_PD){
        queue_id = ID_MSG_URZAD_PD;
        queue_back_id = ID_MSG_URZAD_PD_BACK;
    }
    else if(cel == DEPT_KASA){
        queue_id = ID_MSG_URZAD_KASA;
        queue_back_id = ID_MSG_URZAD_KASA_BACK;
    }
}

void atexit_action(void){
    while(sem_v(semid, SEM_PETENCI));
}
