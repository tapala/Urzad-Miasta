#include "common.h"


int shmid, semid, msg_bilet_id, msg_bilet_back_id, msg_urzad_id, msg_urzad_back_id;
SharedData *shm;
pid_t generator_pid;

void signal_handler(int sig);
void init_ipc(void);
void run_urzednik(int dept, int limit);
void cleanup(void);

int main(){

    signal(SIGINT, signal_handler);
    //srand(time(NULL));
    init_ipc();

    printf("[DYREKTOR] System startuje. Czekamy na Tp - drzwi zamknięte.\n");

    //Urzędnicy:
    if (!fork()) { 
        run_urzednik(DEPT_SA, LIMIT_SA);
        perror("execl SA");
        exit(0); 
    }

    if (!fork()) { 
        run_urzednik(DEPT_SA, LIMIT_SA);
        perror("execl SA");
        exit(0); 
    }

    if (!fork()) { 
        run_urzednik(DEPT_SC, LIMIT_SC);
        perror("execl SC");
        exit(0); 
    }
    
    if (!fork()) { 
        run_urzednik(DEPT_KM, LIMIT_KM);
        perror("execl KM");
        exit(0); 
    }

    if (!fork()) { 
        run_urzednik(DEPT_ML, LIMIT_ML);
        perror("execl ML");
        exit(0); 
    }

    if (!fork()) { 
        run_urzednik(DEPT_PD, LIMIT_PD);
        perror("execl PD");
        exit(0); 
    }

    if (!fork()) { 
        run_urzednik(DEPT_KASA, LIMIT_KASA);
        perror("execl kasa");
        exit(0); 
    }


    //Biletomat:
    if (!fork()) {
        execl("./rejestracja", "rejestracja", NULL);
        perror("execl rejestracja");
        exit(1);
    }


    //Generator petentów:
    generator_pid = fork();                                             //Forkujemy mmaina         
    if (!generator_pid) {                                           //Jeśli dzieciak to dajemy mu przywilej bycia generatorem, aż do śmierci 
        signal(SIGCHLD, signal_handler);
        while (1) {                                                     //Generator pracuje do zamknięcia urzędu
            //Generator pracuje do zamknięcia urzędu
            sem_p(semid, SEM_MUTEX);
            int status = shm->koniec_pracy;
            sem_v(semid, SEM_MUTEX);

            if (status == 1 || status == 2) break;
            //while (waitpid(-1, NULL, WNOHANG) > 0) {}

            // Opuszczamy semafor naszego bufora petentów w celu zapobiegnięcia nadmiernego wykożystania procesora
            struct sembuf s; 
            s.sem_num = SEM_PETENCI; 
            s.sem_op = -1; 
            s.sem_flg = IPC_NOWAIT; 
            if (semop(semid, &s, 1) == -1){
                if(errno == EAGAIN){
                    fprintf(stderr,"%s -- %s -- Generator wykona nie robi wincyj petentow => %d \n", strerror(errno),__FILE__,__LINE__);
                    break;
                }
                else if (errno != EIDRM && errno != EINVAL) {
                    perror("semop");
                }
            }                           
            //usleep(1000);
            pid_t child = fork();                                       //Forkujemy nasz generator
            if (child == -1){
                fprintf(stderr,"%s -- %s -- Wyjebka na Generator => %d \n", strerror(errno),__FILE__,__LINE__);
            }
            if (!child) {   
                unsigned int seed = time(NULL) ^ (getpid() << 16);
                srand(seed);
                int r = rand() % 100;
                int cel;
                if (r < 60) cel = DEPT_SA;
                else if (r < 70) cel = DEPT_SC;
                else if (r < 80) cel = DEPT_KM;
                else if (r < 90) cel = DEPT_ML;
                else cel = DEPT_PD;                                    //Jeśli jest dzieciakiem to przerabiamy go na petenta
                char huj[16];
                //sem_p(semid, SEM_MUTEX);
                //printf("PID: %d; CEL: %d; LIMIT: %d\n", getpid(), cel,shm->limity_przyjec[cel]);
                //fflush(stdout);
                sem_v(semid, SEM_MUTEX);
                snprintf(huj, sizeof(huj), "%d", cel);
                execl("./petent", "petent", huj ,NULL);
                //Jeśli tu jesteśmy, exec jebnął
                perror("execl petent");
                exit(1);
            }

            //printf("Wygenerowano petenta o id = %d\n", child);
            //Gdyby ktoś chciał to jest lekki opóźniacz do generatora petentów
            //usleep((rand() % 400 + 100) * 10);
        }
        while(wait(NULL)>0);
        exit(0);
    }


    //----------- HARMONOGRAM -------------

    sleep(CZAS_DO_OTWARCIA);

    printf("[DYREKTOR] Tp — otwieramy drzwi.");
    fflush(stdout);

    //Wpuszczamy N osób na semaforze budynku
    sem_op(semid, SEM_BUDYNEK, MAX_PETENTOW_W_BUDYNKU, 0);

    //Praca do Tk
    sleep(CZAS_PRACY);

    printf("[DYREKTOR] Tk — zamykamy, nowi nie wchodzą.\n");
    fflush(stdout);

    sem_p(semid, SEM_MUTEX);
    shm->koniec_pracy = 1;   //Zamknięcie, ale bez ewakuacji
    sem_v(semid, SEM_MUTEX);

    sleep(CZAS_PO_ZAMKNIECIU);
    printf("[DYREKTOR] Frustracja po podanym czasie\n");
    fflush(stdout);
    printf("[DYREKTOR] Ewakuacja logiczna.\n");
    fflush(stdout);

    sem_p(semid, SEM_MUTEX);
    shm->koniec_pracy = 2;
    sem_v(semid, SEM_MUTEX);

    //Funkcja do czyszczena
    cleanup();

    //Czekam na dziecki
    //while (wait(NULL) > 0); 

    return 0;
}


//Funkcja tworząca cały syf IPC
void init_ipc() {
    FILE *f = fopen("raport.txt", "w");
    if (f) { fprintf(f, "--- START SYMULACJI ---\n"); fclose(f); }

    //Utworzenie pamięci współdzielonej
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0600 | IPC_CREAT);
    shm = (SharedData*)shmat(shmid, NULL, 0);
    
    //Utworzenie tablicy 3 semaforów i przypisanie im wstępnych wartości
    semid = semget(ftok(FTOK_PATH, ID_SEM), 4, 0600 | IPC_CREAT);
    semctl(semid, SEM_MUTEX, SETVAL, 1);                        //Nasz Mutexik
    semctl(semid, SEM_BUDYNEK, SETVAL, 0);                      //Wpuszczanie do budynku
    semctl(semid, SEM_PETENCI, SETVAL, MAX_PROCESOW_PETENTOW);  //Ogranicznik petentów
    semctl(semid, SEM_MAX_BUDYNEK, SETVAL, (LIMIT_KM + LIMIT_ML + LIMIT_PD + LIMIT_SA + LIMIT_SC));

    //Utworzenie kolejek komunikatów
    msg_bilet_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0600 | IPC_CREAT);
    msg_bilet_back_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET_BACK), 0600 | IPC_CREAT);
    msg_urzad_id = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0600 | IPC_CREAT);
    msg_urzad_back_id = msgget(ftok(FTOK_PATH, ID_MSG_URZAD_BACK), 0600 | IPC_CREAT);

    
    //Ustawiamy wartości pamięci współdzielonej na nasze stałe
    shm->liczba_aktywnych_biletomatow = 1;
    shm->limity_przyjec[DEPT_SA] = LIMIT_SA;
    shm->limity_przyjec[DEPT_SC] = LIMIT_SC;
    shm->limity_przyjec[DEPT_KM] = LIMIT_KM;
    shm->limity_przyjec[DEPT_ML] = LIMIT_ML;
    shm->limity_przyjec[DEPT_PD] = LIMIT_PD;
    shm->limity_przyjec[DEPT_KASA] = LIMIT_KASA; //Dodane w celu testowania - jak się będzie psuć to do wywalenia

    //Zerujemy wartości
    shm->liczba_petentow_w_budynku = 0;
    shm->kolejka_do_biletow = 0;
    shm->koniec_pracy = 0;
}

//Funkcja do forkowania urzędników
void run_urzednik(int dept, int limit) {
    char dept_str[16];
    char limit_str[16];

    snprintf(dept_str, sizeof(dept_str), "%d", dept);
    snprintf(limit_str, sizeof(limit_str), "%d", limit);

    execl("./urzednik", "urzednik", dept_str, limit_str, NULL);

    perror("execl urzednik");
    exit(1);
}


//Funkcja czyszcząca
void cleanup() {
    //Odłączenie pamięci
    if (shm != NULL)
        shmdt(shm);

    shmctl(shmid, IPC_RMID, NULL);              //Usunięcie segmentu pamięci współ.
    semctl(semid, 0, IPC_RMID);                 //Usunięcie semaforów
    msgctl(msg_bilet_id, IPC_RMID, NULL);

    //Usuwanie kolejek komunikatów
    msgctl(msg_urzad_id, IPC_RMID, NULL);
    msgctl(msg_urzad_back_id, IPC_RMID, NULL);

    // zabij generator jeśli jeszcze działa
    if (generator_pid > 0)
        kill(generator_pid, SIGKILL);

    // czekamy na wszystkie dzieciaczki
    while (wait(NULL) > 0);
}


// Obsługa sygnałów
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n[DYREKTOR] SIGINT -> ewakuacja.\n");
        if (shm)
            shm->koniec_pracy = 2;
    }
    else if (sig == SIGCHLD){
        wait(NULL);
    }
}
