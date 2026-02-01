#include "common.h"
#include <ncurses.h>

int kolejka_biletowa, limity[7], status, obslozeni[7], vipy[7], semid;

SharedData *shm;
MonitorData *mshm;
void monitor_loop();

int main(){
    initscr();
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(SharedData), 0);
    int mshmid = shmget(ftok(FTOK_PATH, ID_SHM_MONITOR), sizeof(MonitorData), 0);

    if (shmid == -1 || mshmid == -1) {
        perror("shmget failed");
        exit(1);
    }


    if ((shm = (SharedData*)shmat(shmid, NULL, 0)) == (void*)-1 || (mshm = (MonitorData*)shmat(mshmid, NULL, 0)) == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }
    semid = semget(ftok(FTOK_PATH, ID_SEM), 6, 0);
    
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }

    monitor_loop();

    if(shmdt(shm) == -1 || shmdt(mshm)){
        perror("shmdt failed");
        exit(1);
    }
    exit(0);
}

void refresh_data(){
    sem_p_mutex(semid);
    kolejka_biletowa = shm->kolejka_do_biletow;
    memcpy(limity, shm->limity_przyjec, sizeof limity);
    status = shm->koniec_pracy;
    sem_v_mutex(semid);

    while(sem_p(semid, SEM_MONITOR_MUTEX));
    memcpy(obslozeni, mshm->obslozeni, sizeof obslozeni);
    memcpy(vipy, mshm->vipy, sizeof vipy);
    while(sem_v(semid, SEM_MONITOR_MUTEX));

    limity[0] = limity[1];
}

void monitor_loop(){
    const char* nazwy_wydzialow[] = {"SA1", "SA2", "SC", "KM", "ML", "PD"};
    const char* statusy[] = {"PRACA", "ZAMKNIECIE", "ZAMKNIECIE", "ZAMKNIECIE"};
    
    while(1){
        refresh_data();
        
        clear();

        mvprintw(0, 0, "=== MONITOR URZEDU ===");
        mvprintw(1, 0, "Czas: %s", ctime(&(time_t){time(NULL)}));
        mvprintw(2, 0, "Kolejka do biletow: %d", kolejka_biletowa);
        mvprintw(3, 0, "Status urzedu: %s", 
                (status >=0 && status <=3) ? statusy[status] : "NIEZNANY");
        
        mvprintw(5, 0, "--- STATYSTYKI WYDZIALOW ---");
        int line = 5;
        for(int i = 0; i <= 5; i++) {
            mvprintw(line++, 0, "%s: limit=%d, obsluzeni=%d (VIP: %d)", nazwy_wydzialow[i], limity[i], obslozeni[i], vipy[i]);
        }
        
        refresh();

        sleep(1);
    }
}
