#include "common.h"


int shmid, semid, msg_bilet_id, msg_urzad_id;
SharedData *shm;
pid_t generator_pid;


int main(){

    //srand(time(NULL));
    init_ipc();

    printf("[DYREKTOR] System startuje. Czekamy na Tp - drzwi zamknięte.");

    //Urzędnicy:
    if (!fork()) { 
        start_urzednik(DEPT_SA, LIMIT_SA);
        perror("execl SA");
        exit(0); 
    }

    if (!fork()) { 
        start_urzednik(DEPT_SA, LIMIT_SA);
        perror("execl SA");
        exit(0); 
    }

    if (!fork()) { 
        start_urzednik(DEPT_SC, LIMIT_SC);
        perror("execl SC");
        exit(0); 
    }
    
    if (!fork()) { 
        start_urzednik(DEPT_KM, LIMIT_KM);
        perror("execl KM");
        exit(0); 
    }

    if (!fork()) { 
        start_urzednik(DEPT_ML, LIMIT_ML);
        perror("execl ML");
        exit(0); 
    }

    if (!fork()) { 
        start_urzednik(DEPT_PD, LIMIT_PD);
        perror("execl PD");
        exit(0); 
    }

    if (!fork()) { 
        start_urzednik(DEPT_KASA, LIMIT_KASA);
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
    generator_pid = fork();                             //Forkujemy mmaina         
    if (generator_pid == 0) {                           //Jeśli dzieciak to dajemy mu przywilej bycia generatorem, aż do śmierci 

        while (shm->koniec_pracy != 1) {                //Generator pracuje do zamknięcia urzędu

            sem_p(semid, SEM_PETENCI);                  //Opuszczamy semafor naszego bufora petentów w celu zapobiegnięcia nadmiernego wykożystania procesora

            pid_t child = fork();                       //Forkujemy nasz generator
            if (child == 0) {                           //Jeśli jest dzieciakiem to przerabiamy go na petenta
                execl("./petent", "petent", NULL);
                //Jeśli tu jesteśmy, exec jebnął
                perror("execl petent");
                exit(1);
            }

            //Gdyby ktoś chciał to jest lekki opóźniacz do generatora petentów
            //usleep((rand() % 400 + 100) * 10);
        }

        exit(0);
    }


    //----------- HARMONOGRAM -------------

    sleep(CZAS_DO_OTWARCIA);

    printf("[DYREKTOR] Tp — otwieramy drzwi.");

    //Wpuszczamy N osób na semaforze budynku
    sem_op(semid, SEM_BUDYNEK, MAX_PETENTOW_W_BUDYNKU);

    //Praca do Tk
    sleep(CZAS_PRACY);

    printf("[DYREKTOR] Tk — zamykamy, nowi nie wchodzą.\n");

    sem_p(semid, SEM_MUTEX);
    shm->koniec_pracy = 1;   //Zamknięcie, ale bez ewakuacji
    sem_v(semid, SEM_MUTEX);

    printf("[DYREKTOR] Frustracja po podanym czasie");
    sleep(CZAS_PO_ZAMKNIECIU);

    printf("[DYREKTOR] Ewakuacja logiczna.\n");
    shm->koniec_pracy = 2;

    //Czekam na dziecki
    while (wait(NULL) > 0); 

    //Funkcja do czyszczena
    cleanup()

    return 0;
}


//Funkcja tworząca cały syf IPC
void init_ipc() {
    FILE *f = fopen("raport.txt", "w");
    if (f) { fprintf(f, "--- START SYMULACJI ---\n"); fclose(f); }

    //Utworzenie pamięci współdzielonej
    shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0666 | IPC_CREAT);
    shm = (SharedData*)shmat(shmid, NULL, 0);
    
    //Utworzenie tablicy 3 semaforów i przypisanie im wstępnych wartości
    semid = semget(ftok(FTOK_PATH, ID_SEM), 3, 0666 | IPC_CREAT);
    semctl(semid, SEM_MUTEX, SETVAL, 1);                        //Nasz Mutexik
    semctl(semid, SEM_BUDYNEK, SETVAL, 0);                      //Wpuszczanie do budynku
    semctl(semid, SEM_PETENCI, SETVAL, MAX_PROCESOW_PETENTOW);  //Ogranicznik petentów

    //Utworzenie kolejek komunikatów
    msg_bilet_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0666 | IPC_CREAT);
    msg_urzad_id = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0666 | IPC_CREAT);

    
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
void start_urzednik(const char *dept, int limit) {
    if (!fork()) {
        char lim[10];
        sprintf(lim, "%d", limit);
        execl("./urzednik", "urzednik", dept, lim, NULL);
        perror("execl urzednik");
        exit(1);
    }
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

    // zabij generator jeśli jeszcze działa
    if (generator_pid > 0)
        kill(generator_pid, SIGKILL);

    // czekamy na wszystkie dzieciaczki
    while (wait(NULL) > 0);
}


// Funkcje semaforów 
void sem_op(int semid, int sem_num, int op) { 
    struct sembuf s; 
    s.sem_num = sem_num; 
    s.sem_op = op; 
    s.sem_flg = 0; 
    if (semop(semid, &s, 1) == -1){
        if (errno != EIDRM && errno != EINVAL) {
            perror("semop");
        }
    }
} 
void sem_p(int semid, int sem_num) { sem_op(semid, sem_num, -1); } 
void sem_v(int semid, int sem_num) { sem_op(semid, sem_num, 1); }
// Logowanie do pliku
void log_to_file(const char *msg) { 
    FILE *f = fopen("raport.txt", "a"); 
    if (f) { 
        fprintf(f, "%s", msg); 
        fclose(f); 
    } 
}W