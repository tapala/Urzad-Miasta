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
#include <signal.h> 
#include <errno.h> 
#include <time.h> 
#include <string.h> 
#include <sys/wait.h>

#define MAX_PETENTOW_W_BUDYNKU (15000)
#define PROG_URUCHOMIENIA_KAS (350)
#define CZAS_DO_OTWARCIA (0) //Tp
#define CZAS_PRACY (6) //Tk-Tp
#define CZAS_PO_ZAMKNIECIU (5) //Domślnie 2 minuty wedle założeń projektu
#define MAX_PROCESOW_PETENTOW (15000) //Będzie przydatne do ograniczenia petentów w generatorze
#define LIMIT_OSIAGNIETY (-1) //Stała do msgqueue dla czytelności 
#define POWROT_Z_KASY (-2) //Stała do msgqueue
#define KONIEC_OBSLUGI (-3) //Stała do msgqueue

//Limity wydziałow:
#define LIMIT_SA (6000)
#define LIMIT_SC (1000)
#define LIMIT_KM (1000)
#define LIMIT_ML (1000)
#define LIMIT_PD (1000)
#define LIMIT_KASA (999999999)

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

#define ID_MSG_URZAD (13) //Kolejka Kpnikatów - Petent => Urzędnik
#define ID_MSG_URZAD_BACK (14) //Kolejka Kpnikatów - Petent <= Urzędnik

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

//Identyfikatory semaforów
#define SEM_MUTEX (0) // Chroni pamięć dzieloną
#define SEM_BUDYNEK (1) //Semafor na 'wpuszczanie' petentów do budynku
#define SEM_PETENCI (2) //Semafor ograniczający istniejących procesów petentów
#define SEM_LOG_MUTEX (3)

//Pamięć współdzielona:
typedef struct { 
    int kolejka_do_biletow; // Ilu petentów czeka przy biletomacie 
    int limity_przyjec[7]; //Limity na wydział - liczba jest 7, bo rozpisując schemat na kartce będąc pod ostrym wpływem absyntu zmieszanego ze śliwowicą(na sylwestrze się działo) 'zapomniałem', że tablica leci od 0, a potem tak zostawiłem, bo w sumie śmiesznie  
    int limity_przyjec_sum;
    int koniec_pracy; // 0 - Zakład pracuje, 1 - Zamknięcie(po CZAS_PRACY), 2 - Ewakuacja 
    int liczba_aktywnych_biletomatow; 
    int sa_zamkiete;
    int brak_petentow;
} SharedData;

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
} Komunikat;

// Deklaracje funkcji pomocniczych  
void log_to_file(const char *msg, int semid); 
void sem_p(int semid, int sem_num); 
void sem_v(int semid, int sem_num); 
void sem_op(int semid, int sem_num, int op, short flag);
int semt_p(int semid, int sem_num, long nanotime); 
int semt_v(int semid, int sem_num, long nanotime); 
int semt_op(int semid, int sem_num, int op, long nanotime);
void gotowanie_procesora(int seconds);
#endif