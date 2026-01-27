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

void dzong(){
    fprintf(stderr,"[%d] Im dead\n", getpid());
}

int main(int argc, char** argv) {
    //atexit(dzong);
    unsigned int seed = time(NULL) ^ (getpid() << 16);
    srand(seed);
    cel = atoi(argv[1]);
    set_queue_id();
    // Inicjalizacja handlerów pamięci współdzielonej i kolejki komunikatów
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);

    semid = semget(ftok(FTOK_PATH, ID_SEM), 5, 0);
    msg_bilet_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0);
    msg_bilet_back_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0);
    msg_urzad_id = msgget(ftok(FTOK_PATH, queue_id), 0);
    msg_urzad_back_id = msgget(ftok(FTOK_PATH, queue_back_id), 0600);

    petent_loop();

    return 0;
}

void* opiekun_thread(void *arg)
{
    (void)arg;
    // Wejście do kolejki biletowej
    sem_p(semid, SEM_MUTEX);
    shm->kolejka_do_biletow++;
    sem_v(semid, SEM_MUTEX);

    // Wysłanie żądania biletu
    if(msgsnd(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
        fprintf(stderr,"%s - Wyjebka na msgsnd - Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
    }

    // Odebranie biletu adresowanego do PID petenta
    if(msgrcv(msg_bilet_back_id, &msg, sizeof(Komunikat) - sizeof(long), msg.pid_petenta, 0) == -1){
        fprintf(stderr,"%s - Wyjebka na msgrcv - Rejestracja - %s => %d \n", strerror(errno), __FILE__,__LINE__);
    }

    if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
        printf("\033[41m[PETENT %d] Brak miejsc u urzednika %d. Z zalu popelniam sudoku\033[m\n", my_pid, cel);
        fflush(stdout);
        sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca, 0);
        exit(1);
    }

    // Informacja dla dziecka - wręczenie biletu
    pthread_mutex_lock(&mtx);
    bilet_gotowy = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mtx);

    return NULL;
}

void petent_loop() {

    // Pamięć współdzielona
    shm = (SharedData*)shmat(shmid, NULL, 0);

    // Ustawiamy/losujemy początkowe dane
    my_pid = getpid();
    //int cel = 0;
    int vip = (rand() % 100 < 2);

    // Obsługa wieku
    int wiek = (rand() % 90) + 1;               // Losujemy Wiek
    int wiek_opiekuna = 0;

    if (wiek < 18) {                            // Jeśli dziecko to losujemy wiek opiekuna i ustawiamy zajmowane miejsce na 2
        wiek_opiekuna = (rand() % 60) + 18;
        zajmowane_miejsca = 2;
    }

    sem_p(semid, SEM_MUTEX);
    if(!shm->limity_przyjec_sum){
        sem_v(semid, SEM_MUTEX);
        printf("\033[44m\033[33m[PETENT %d] Brak miejsc u urzedników. Z zalu popelniam sudoku\033[m\n", my_pid);
        fflush(stdout);
        shmdt(shm);
        exit(1);
    }
    sem_v(semid, SEM_MUTEX);

    sem_p(semid, SEM_MUTEX);
    int status = shm->koniec_pracy;

    sem_v(semid, SEM_MUTEX);

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

        pthread_t th;
        pthread_create(&th, NULL, opiekun_thread, NULL);

        // Dziecko czeka na rodzica z biletem
        pthread_mutex_lock(&mtx);

        while (!bilet_gotowy)
            pthread_cond_wait(&cond, &mtx);

        pthread_mutex_unlock(&mtx);

        pthread_join(th, NULL);
    }
    // Dorosły działa jak wcześniej
    else {
        
        sem_p(semid, SEM_MUTEX);
        shm->kolejka_do_biletow++;
        sem_v(semid, SEM_MUTEX);

        // Komunikat wysyłamy do do biletomatu...
        if(msgsnd(msg_bilet_id, &msg, sizeof(Komunikat) - sizeof(long), 0) == -1){
            fprintf(stderr,"%s -- %d -- Wyjebka na msgsend - Rejestracja - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
            kys();
        }

        // ...po czym pobieramy komunikat zwrotny...
        if(msgrcv(msg_bilet_back_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1){
            fprintf(stderr,"%s -- %d -- Wyjebka na msgrcv - Rejestracja - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
            kys();
        }

        if(msg.typ_sprawy == LIMIT_OSIAGNIETY){
            sem_p(semid, SEM_MUTEX);
            shm->kolejka_do_biletow--;
            sem_v(semid, SEM_MUTEX);
            printf("\033[41m[PETENT %d] Brak miejsc u urzednika %d. Z zalu popelniam sudoku\033[m\n", my_pid, cel);
            fflush(stdout);
            kys();
        }

    }

    // ...a na koniec zmniejszamy piczbę petentów w kolejce o 1
    sem_p(semid, SEM_MUTEX);
    shm->kolejka_do_biletow--;
    sem_v(semid, SEM_MUTEX);

    while (1) {

        sem_p(semid, SEM_MUTEX);
        int status = shm->koniec_pracy;
        sem_v(semid, SEM_MUTEX);

        if (status == 2) break;

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
            fprintf(stderr,"%s -- %d -- Wyjebka na msgsend - Urzednik - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
            kys();
        }

        // Jeśli komunikat się nie powiedzie to przerywamy pętlę
        if (msgrcv(msg_urzad_back_id, &msg, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1){
            fprintf(stderr,"%s -- %d -- Wyjebka na msgrcv - Urzednik - %s => %d \n", strerror(errno), my_pid,__FILE__,__LINE__);
            break;
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
            printf("\033[46m\033[34m[PETENT %d] Chcę rozmawiać z menadżerem                       \033[m\n", my_pid);
            fflush(stdout);
            break;
        }
        // W przeciwnym razie urzędnik SA nas odesłał do innego urzędnika i musimy powtórzyć proces nadpisując cel(wszystko jest robione na referencji do wiadomości)
        else{
            cel = msg.typ_sprawy;
            set_queue_id();
            msg_urzad_id = msgget(ftok(FTOK_PATH, queue_id), 0);
            msg_urzad_back_id = msgget(ftok(FTOK_PATH, queue_back_id), 0);
        }
    }

    // Petent obsłużony wychodzi z budynku

    sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca, 0);

    shmdt(shm);
}

void kys(){
    sem_op(semid, SEM_BUDYNEK, zajmowane_miejsca, 0);

    shmdt(shm);
    exit(1);
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