#ifndef COMMON_H 
#define COMMON_H

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/sem.h> 
#include <sys/msg.h>
#include <sys/time.h> 
#include <sys/resource.h>
#include <signal.h> 
#include <errno.h> 
#include <time.h> 
#include <string.h> 
#include <sys/wait.h>

#define MAX_PETENTOW_W_BUDYNKU (1000)
#define PROG_URUCHOMIENIA_KAS (750)
#define CZAS_DO_OTWARCIA (5) //Tp
#define CZAS_PRACY (10) //Tk-Tp
#define CZAS_PO_ZAMKNIECIU (0) //Domślnie 2 minuty wedle założeń projektu
#define MAX_PROCESOW_PETENTOW (10000) //Będzie przydatne do ograniczenia petentów w generatorze

#define LIMIT_OSIAGNIETY (-1) //Stała do msgqueue dla czytelności 
#define POWROT_Z_KASY (-2) //Stała do msgqueue
#define KONIEC_OBSLUGI (-3) //Stała do msgqueue

//Limity wydziałow:
#define LIMIT_SA (6000)
#define LIMIT_SC (1000)
#define LIMIT_KM (1000)
#define LIMIT_ML (1000)
#define LIMIT_PD (1000)
#define MAX_BILETOW (LIMIT_SA + LIMIT_SC + LIMIT_KM + LIMIT_ML +LIMIT_PD)

//Identyfikatory wydziałow i kasy - przyda się do obsługi p[etenta]
#define DEPT_SA (1)
#define DEPT_SC (2)
#define DEPT_KM (3)
#define DEPT_ML (4)
#define DEPT_PD (5)
#define DEPT_KASA (6)

//Klucze IPC wraz z generatrem
#define FTOK_PATH "."
#define ID_SHM (9) //Pamięć współdzielona
#define ID_SEM (10) //Teblica Semaforów
#define ID_MSG_BILET (11) //Kolejka Komunikatów - Penent(lub SA) => Biletomat
#define ID_MSG_BILET_BACK (12) //Kolejka Komunikatów - Petent <= Biletomat

#define ID_MSG_URZAD_SA (13) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_SA_BACK (14) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_MSG_URZAD_SC (15) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_SC_BACK (16) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_MSG_URZAD_KM (17) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_KM_BACK (18) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_MSG_URZAD_ML (19) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_ML_BACK (20) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_MSG_URZAD_PD (21) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_PD_BACK (22) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_MSG_URZAD_KASA (23) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_KASA_BACK (24) //Kolejka Kpnikatów - Petent <= Urzędnik

#define ID_SHM_MONITOR (25)

//Identyfikatory semaforów
#define SEM_MUTEX (0) // Chroni pamięć dzieloną
#define SEM_BUDYNEK (1) //Semafor na 'wpuszczanie' petentów do budynku
#define SEM_PETENCI (2) //Semafor ograniczający istniejących procesów petentów
#define SEM_LOG_MUTEX (3)
#define SEM_LOCK_REGISTER (4)
#define SEM_MONITOR_MUTEX (5)

//Pamięć współdzielona:
typedef struct { 
    int kolejka_do_biletow; // Ilu petentów czeka przy biletomacie 
    int limity_przyjec[7]; //Limity na wydział - liczba jest 7, bo rozpisując schemat na kartce będąc pod ostrym wpływem absyntu zmieszanego ze śliwowicą(na sylwestrze się działo) 'zapomniałem', że tablica leci od 0, a potem tak zostawiłem, bo w sumie śmiesznie  
    int limity_przyjec_sum;
    int koniec_pracy; // 0 - Zakład pracuje, 1 - Zamknięcie(po CZAS_PRACY), 2 - Ewakuacja 
    int sa_zamkiete;
} SharedData;

typedef struct { 
    int obslozeni[7]; 
    int vipy[7];
} MonitorData;

//Kolejka komunikatów:
//Na początku miały być 2 różne(dla ID_MSG_URZAD i ID_MSG_BILET), ale doszedłem do wniosku, że czytelniej będzie zrobić 1, bo tak w sumie to się prawie całkowicie pokrywają
typedef struct { 
    long mtype; // PID petenta lub ID wydziału 
    pid_t pid_petenta; //Numer wydziału / informacja o przekierowaniu
    int typ_sprawy; // Cel wizyty 
    int jest_vip; // Binarka 
    int odeslany_z_sa; // Flaga przekierowania - żeby się nie zapętlało
    int cel_po_kasie; // Jeśli wraca z kasy to musi wiedzieć do kogo wrócić
    int kasa_odwiedzona; // Flaga informująca, że petent był już w kasie
    int wiek; // Wiek petenta 
    int wiek_opiekuna; // >25 jeśli petent <18, wedle tych przeklętych wymagań
    int nr_biletu; // Bo tak
} Komunikat;

// Deklaracje funkcji pomocniczych  
void log_to_file(const char *msg, int semid); 
int sem_p(int semid, int sem_num); 
int sem_v(int semid, int sem_num); 
int sem_op(int semid, int sem_num, int op, short flag);
void gotowanie_procesora(int seconds);
key_t ftok_handler(const char *path, int proj_id);
int msgget_CREATE_handler(const char *path, int proj_id);
void semctl_SETVAL_handler(int semid, int sem_name,int value);
void msgctl_IPC_RMID_handler(int msqid);
void init_signals(void);
void block_signal(void);
void restore_signal(void);
void sem_p_mutex(int semid);
void sem_v_mutex(int semid);
#endif