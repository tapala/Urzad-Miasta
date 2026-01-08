#include "common.h"


int shmid, semid, msg_bilet_id, msg_urzad_id;
SharedData *shm;
pid_t generator_pid;

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


    msg_bilet_id = msgget(ftok(FTOK_PATH, ID_MSG_BILET), 0666 | IPC_CREAT);
    msg_urzad_id = msgget(ftok(FTOK_PATH, ID_MSG_URZAD), 0666 | IPC_CREAT);

    
    //Ustawiamy wartości pamięci współdzielonej na nasze stałe
    shm->liczba_aktywnych_biletomatow = 1;
    shm->limity_przyjec[DEPT_SA] = LIMIT_SA;
    shm->limity_przyjec[DEPT_SC] = LIMIT_SC;
    shm->limity_przyjec[DEPT_KM] = LIMIT_KM;
    shm->limity_przyjec[DEPT_ML] = LIMIT_ML;
    shm->limity_przyjec[DEPT_PD] = LIMIT_PD;
    shm->limity_przyjec[DEPT_KASA] = 99999999; //Dodane w celu testowania - jak się będzie psuć to do wywalenia

    //Zerujemy wartości
    shm->liczba_petentow_w_budynku = 0;
    shm->kolejka_do_biletow = 0;
    shm->koniec_pracy = 0;
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