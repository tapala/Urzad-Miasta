#include "common.h"


int shmid, semid, msg_bilet_id, msg_bilet_back_id, msg_urzad_id, msg_urzad_back_id, generator_stop_flag, msg_urzad_SA_id, msg_urzad_SA_back_id, msg_urzad_SC_id, msg_urzad_SC_back_id, msg_urzad_KM_id, msg_urzad_KM_back_id, msg_urzad_ML_id, msg_urzad_ML_back_id, msg_urzad_PD_id, msg_urzad_PD_back_id, msg_urzad_KASA_id, msg_urzad_KASA_back_id;
int mshmid;
MonitorData *mshm;
SharedData *shm;
pid_t generator_pid;
pid_t urzednicy[6];

void signal_handler(int sig);
void input_validation();
void init_ipc(void);
void run_urzednik(int dept, int offset);
void cleanup(void);
void sojowanie_urzednikow(pid_t pidus_amogus_susus_impostorus, int type);
void generator();
void zamykanie();

int main(){
    input_validation();
    init_signals();

    if (signal(SIGINT, signal_handler) == SIG_ERR || signal(SIGRTMIN, signal_handler) == SIG_ERR || signal(SIGUSR2, signal_handler) == SIG_ERR){
        perror("signal main");
        exit(1);
    }

    generator_stop_flag = 0;
    init_ipc();

    printf("[DYREKTOR] System startuje. Czekamy na Tp - drzwi zamknięte.\n");

    //Urzędnicy:

    urzednicy[0] = fork();
    if (!urzednicy[0]) { 
        run_urzednik(DEPT_SA, 1);
        perror("execl SA");
        exit(1); 
    }
    else if(urzednicy[0] == -1){
        perror("fork SA");
        exit(1);
    }

    urzednicy[1] = fork();
    if (!urzednicy[1]) { 
        run_urzednik(DEPT_SA, 0);
        perror("execl SA");
        exit(1); 
    }
    else if(urzednicy[1] == -1){
        perror("fork SA");
        exit(1);
    }

    urzednicy[2] = fork();
    if (!urzednicy[2]) { 
        run_urzednik(DEPT_SC, 0);
        perror("execl SC");
        exit(1); 
    }
    else if(urzednicy[2] == -1){
        perror("fork SC");
        exit(1);
    }
    
    urzednicy[3] = fork();
    if (!urzednicy[3]) { 
        run_urzednik(DEPT_KM, 0);
        perror("execl KM");
        exit(1); 
    }
    else if(urzednicy[3] == -1){
        perror("fork KM");
        exit(1);
    }
    
    urzednicy[4] = fork();
    if (!urzednicy[4]) { 
        run_urzednik(DEPT_ML, 0);
        perror("execl ML");
        exit(1); 
    }
    else if(urzednicy[4] == -1){
        perror("fork ML");
        exit(1);
    }
    
    urzednicy[5] = fork();
    if (!urzednicy[5]) { 
        run_urzednik(DEPT_PD, 0);
        perror("execl PD");
        exit(1); 
    }
    else if(urzednicy[5] == -1){
        perror("fork PD");
        exit(1);
    }
    
    urzednicy[6] = fork();
    if (!urzednicy[6]) { 
        run_urzednik(DEPT_KASA, 0);
        perror("execl kasa");
        exit(1); 
    }
    else if(urzednicy[6] == -1){
        perror("fork kasa");
        exit(1);
    }
    

    //Biletomat:
    int biletomat = fork();
    if (!biletomat) {
        execl("./rejestracja", "rejestracja", NULL);
        perror("execl rejestracja");
        exit(1);
    }
    else if(biletomat == -1){
        perror("fork biletomat");
        exit(1);
    }


    //Generator petentów:
    generator_pid = fork();                                             //Forkujemy mmaina         
    if(generator_pid == -1){
        perror("fork generator");
        exit(1);
    }
    generator();


    //----------- HARMONOGRAM -------------

    gotowanie_procesora(CZAS_DO_OTWARCIA);

    printf("[DYREKTOR] Tp — otwieramy drzwi.\n");
    fflush(stdout);

    //Wpuszczamy N osób na semaforze budynku
    while(sem_op(semid, SEM_BUDYNEK, MAX_PETENTOW_W_BUDYNKU, 0));

    //Praca do Tk
    gotowanie_procesora(CZAS_PRACY);
    zamykanie();
    return 0;
}


//Funkcja tworząca cały syf IPC
void init_ipc() {
    FILE *f = fopen("raport.txt", "w");
    if (f) { 
        fprintf(f, "--- START SYMULACJI ---\n"); 
        fclose(f); 
    }
    else{
        perror("fopen");
        exit(1);
    }

    //Utworzenie pamięci współdzielonej
    shmid = shmget(ftok_handler(FTOK_PATH, ID_SHM), sizeof(SharedData), 0600 | IPC_CREAT);
    mshmid = shmget(ftok_handler(FTOK_PATH, ID_SHM_MONITOR), sizeof(MonitorData), 0600 | IPC_CREAT);
    if(shmid == -1|| mshmid == -1){
        perror("shmget main");
        exit(1);
    }
    
    shm = (SharedData*)shmat(shmid, NULL, 0);
    mshm = (MonitorData*)shmat(mshmid, NULL, 0);
    if(shm == (void*)-1|| mshm == (void*)-1){
        perror("shmget main");
        exit(1);
    }

    //Utworzenie tablicy 3 semaforów i przypisanie im wstępnych wartości
    semid = semget(ftok_handler(FTOK_PATH, ID_SEM), 6, 0600 | IPC_CREAT);
    if(semid == -1){
        perror("semget main");
        exit(1);
    }
    semctl_SETVAL_handler(semid, SEM_MUTEX, 1);
    semctl_SETVAL_handler(semid, SEM_BUDYNEK, 0);
    semctl_SETVAL_handler(semid, SEM_PETENCI, MAX_PROCESOW_PETENTOW);
    semctl_SETVAL_handler(semid, SEM_LOG_MUTEX, 1);
    semctl_SETVAL_handler(semid, SEM_LOCK_REGISTER, 0);
    semctl_SETVAL_handler(semid, SEM_MONITOR_MUTEX, 1) ;                                                 

    //Utworzenie kolejek komunikatów
    msg_bilet_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_BILET);
    msg_bilet_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_BILET_BACK);

    msg_urzad_SA_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_SA);
    msg_urzad_SA_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_SA_BACK);

    msg_urzad_SC_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_SC);
    msg_urzad_SC_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_SC_BACK);

    msg_urzad_KM_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_KM);
    msg_urzad_KM_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_KM_BACK);

    msg_urzad_ML_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_ML);
    msg_urzad_ML_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_ML_BACK);

    msg_urzad_PD_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_PD);
    msg_urzad_PD_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_PD_BACK);

    msg_urzad_KASA_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_KASA);
    msg_urzad_KASA_back_id = msgget_CREATE_handler(FTOK_PATH, ID_MSG_URZAD_KASA_BACK);
    
    //Ustawiamy wartości pamięci współdzielonej na nasze stałe
    shm->limity_przyjec[DEPT_SA] = LIMIT_SA;
    shm->limity_przyjec[DEPT_SC] = LIMIT_SC;
    shm->limity_przyjec[DEPT_KM] = LIMIT_KM;
    shm->limity_przyjec[DEPT_ML] = LIMIT_ML;
    shm->limity_przyjec[DEPT_PD] = LIMIT_PD;
    shm->limity_przyjec[DEPT_KASA] = 999999999;
    shm->limity_przyjec_sum = MAX_BILETOW;

    //Zerujemy wartości
    shm->kolejka_do_biletow = 0;
    shm->koniec_pracy = 0;
    shm->sa_zamkiete = 0;

    memset(mshm->obslozeni, 0, sizeof(mshm->obslozeni));
    memset(mshm->vipy, 0, sizeof(mshm->vipy));
}

//Funkcja do forkowania urzędników
void run_urzednik(int dept, int offset) {
    char dept_str[16];
    char offset_str[16];

    snprintf(dept_str, sizeof(dept_str), "%d", dept);
    snprintf(offset_str, sizeof(offset_str), "%d", offset);

    execl("./urzednik", "urzednik", dept_str, offset_str, NULL);
    perror("execl urzednik");
    exit(1);
}


//Funkcja czyszcząca
void cleanup() {

    //Odłączenie pamięci
    if (shm != NULL){
        if(shmdt(shm)){
            perror("shmdt shm main");
        }
    }

    if (mshm != NULL){
        if(shmdt(mshm)){
            perror("shmdt mshm main");
        }
    }

    if(shmctl(mshmid, IPC_RMID, NULL) == -1){
        perror("shmctl IPC_RMID mshmid");
        exit(1);
    }
    if(shmctl(shmid, IPC_RMID, NULL) == -1){
        perror("shmctl IPC_RMID shmid");
        exit(1);
    }
    if(semctl(semid, 0, IPC_RMID) == -1){
        perror("semctl IPC_RMID");
        exit(1);
    }

    msgctl_IPC_RMID_handler(msg_bilet_id);
    msgctl_IPC_RMID_handler(msg_bilet_back_id);

    // czekamy na wszystkie dzieciaczki
    while (wait(NULL) > 0);
}


// Obsługa sygnałów
void signal_handler(int sig) {
    if (sig == SIGCHLD){
        wait(NULL);
    }
    else if (sig == SIGTERM){
        generator_stop_flag = 1;
    }
    else if(sig == SIGUSR2){
        if(generator_pid){
            if(kill(0, SIGRTMIN) == -1){
                perror("kill SIGUSR2");
            }
        }
    }
    else if(sig == SIGRTMIN){
        if(!generator_pid){
            generator_stop_flag = 1;
        }
    }
}

void sojowanie_urzednikow(pid_t pidus_amogus_susus_impostorus, int type){
    if(type == 1){
        if(kill(pidus_amogus_susus_impostorus, SIGUSR1) == -1){
                perror("kill sojowanie SIGUSR1");
        }
    }
    else{
        if(kill(pidus_amogus_susus_impostorus, SIGUSR2) == -1){
                perror("kill sojowanie SIGUSR1");
        }
    }
}

void generator(){
    if (!generator_pid) {                                           //Jeśli dzieciak to dajemy mu przywilej bycia generatorem, aż do śmierci 
        if(signal(SIGCHLD, signal_handler) == SIG_ERR || signal(SIGTERM, signal_handler) == SIG_ERR || signal(SIGRTMIN, signal_handler) == SIG_ERR){
            perror("signal generator");
            exit(1);
        }
        init_signals();
        while (1) {                                                     //Generator pracuje do zamknięcia urzędu
            //Generator pracuje do zamknięcia urzędu
            if (generator_stop_flag) break;
            while (waitpid(-1, NULL, WNOHANG) > 0);

            // Opuszczamy semafor naszego bufora petentów w celu zapobiegnięcia nadmiernego wykożystania procesora
            if(sem_p(semid, SEM_PETENCI)) continue;                      
            pid_t child = fork();                                       //Forkujemy nasz generator
            if (child == -1){
                fprintf(stderr,"%s -- %s -- Wywrotka na Generator => %d \n", strerror(errno),__FILE__,__LINE__);
                exit(1);
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
                else cel = DEPT_PD;
                char kamilek[16];

                snprintf(kamilek, sizeof(kamilek), "%d", cel);
                execl("./petent", "petent", kamilek ,NULL);
                perror("execl petent");
                exit(1);
            }
        }
        while(wait(NULL)>0);
        for(int i=0; i<7; i++){
            sojowanie_urzednikow(urzednicy[i], 2);
        }
        while(sem_v(semid, SEM_LOCK_REGISTER));
        exit(0);
    }
}

void zamykanie(){
    printf("[DYREKTOR] Tk — zamykamy, nowi nie wchodzą.\n");
    fflush(stdout);

    sem_p_mutex(semid);
    shm->koniec_pracy = 1;   //Zamknięcie
    sem_v_mutex(semid);

    // Zabij generator jeśli jeszcze działa
    if (generator_pid > 0){
        if(kill(generator_pid, SIGTERM) == -1){
            perror("kill generator");
            exit(1);
        }
    }

    for(int i=0; i<7; i++){
        sojowanie_urzednikow(urzednicy[i], 1);
    }

    while(sem_op(semid, SEM_LOCK_REGISTER, -8, 0));

    sem_p_mutex(semid);
    shm->koniec_pracy = 3;   //Zamknięcie Biletomatów
    sem_v_mutex(semid);

    while (wait(NULL) > 0); 

    //Funkcja do czyszczena
    cleanup();

    //Czekam na dziecki
    while (wait(NULL) > 0); 
}

void input_validation(){
    struct rlimit rlim;
    
    if (getrlimit(RLIMIT_NPROC, &rlim) == -1) {
        perror("getrlimit");
        return;
    }
    int r = (rlim.rlim_cur * 7) / 10;
    if(MAX_PROCESOW_PETENTOW > r){
        printf("Za duży ustawiony limit procesów - MAX_PROCESOW_PETENTOW (ponad %d)\n", r);
        fflush(stdout);
        exit(1);
    }
    if(MAX_PETENTOW_W_BUDYNKU < 2 || MAX_PROCESOW_PETENTOW < 10){
        printf("Zbyt niska MAX_PETENTOW_W_BUDYNKU lub MAX_PROCESOW_PETENTOW\n");
        fflush(stdout);
        exit(1);
    }
    if((MAX_PETENTOW_W_BUDYNKU/3) > PROG_URUCHOMIENIA_KAS){
        printf("Zbyt niski PROG_URUCHOMIENIA_KAS względem MAX_PETENTOW_W_BUDYNKU\n");
        fflush(stdout);
        exit(1);
    }
    if(CZAS_PRACY < 1){
        printf("Zbyt krótki CZAS_PRACY\n");
        fflush(stdout);
        exit(1);
    }
    if(CZAS_DO_OTWARCIA < 0 || CZAS_PO_ZAMKNIECIU < 0 || LIMIT_SA < 0 || LIMIT_SC < 0 || LIMIT_KM < 0 || LIMIT_ML < 0 || LIMIT_PD < 0){
        printf("[UWAGA] CZAS_DO_OTWARCIA, CZAS_PO_ZAMKNIECIU lub któryś z LIMIT'ów jest poniżej 0 \n");
        fflush(stdout);
    }
    if(MAX_PETENTOW_W_BUDYNKU > MAX_PETENTOW_W_BUDYNKU){
        printf("[UWAGA] więcej MAX_PETENTOW_W_BUDYNKU niż w bufferze MAX_PETENTOW_W_BUDYNKU\n");
        fflush(stdout);
    }
}