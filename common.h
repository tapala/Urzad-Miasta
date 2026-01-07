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
#include <signal.h> 
#include <errno.h> 
#include <time.h> 
#include <string.h> 
#include <sys/wait.h>

#define MAX_PETENTOW_W_BUDYNKU 20
#define PROG_URUCHOMIENIA_KAS 8
#define CZAS_DO_OTWARCIA 5 //Tp
#define CZAS_PRACY 20 //Tk-Tp
#define CZAS_PO_ZAMKNIECIU 120 //Domślnie 2 minuty wedle założeń projektu

//Limity wydziałow:
#define LIMIT_SA 15
#define LIMIT_SC 8
#define LIMIT_KM 8
#define LIMIT_ML 8
#define LIMIT_PD 8

//Identyfikatory wydziałow i kasy - przyda się do obsługi p[etenta]
#define DEPT_SA 1
#define DEPT_SC 2
#define DEPT_KM 3
#define DEPT_ML 4
#define DEPT_PD 5
#define DEPT_KASA 6

//Klucze IPC wraz z generatrem
#define FTOK_PATH "."
#define ID_SHM 10 //Pamięć współdzielona
#define ID_SEM 11 //Teblica Semaforów
#define ID_MSG_BILET 12 //Kolejka Komunikatów - Bilety
#define ID_MSG_URZAD 13 //Kolejka Kpnikatów - Petent <=> Urzędnik

//Identyfikatory semaforów
#define SEM_MUTEX 0 // Chroni pamięć dzieloną
#define SEM_BUDYNEK 1 //Semafor na 'wpuszczanie' petentów do budynku

//Pamięć współdzielona:
typedef struct { 
    int liczba_petentow_w_budynku; // Ile osób w budynku 
    int kolejka_do_biletow; // Ilu petentów czeka przy biletomacie 
    int limity_przyjec[7]; //Limity na wydział - liczba jest 7, bo rozpisując schemat na kartce będąc pod ostrym wpływem absyntu zmieszanego ze śliwowicą(na sylwestrze się działo) 'zapomniałem', że tablica leci od 0, a potem tak zostawiłem, bo w sumie śmiesznie  
    int koniec_pracy; // 0 - Zakład pracuje, 1 - Zamknięcie(po CZAS_PRACY), 2 - Ewakuacja 
    int liczba_aktywnych_biletomatow; 
} SharedData;

//Kolejka komunikatów:
//Na początku miały być 2 różne(dla ID_MSG_URZAD i ID_MSG_BILET), ale doszedłem do wniosku, że czytelniej będzie zrobić 1, bo tak w sumie to się prawie całkowicie pokrywają
typedef struct { 
    long mtype; // PID petenta lub ID wydziału 
    pid_t pid_petenta; //Numer wydziału / informacja o przekierowaniu
    int typ_sprawy; // Cel wizyty 
    int jest_vip; // Binarka 
    int odeslany_z_sa; // Flaga przekierowania - żeby się nie zapętlało

    int wiek; // Wiek petenta 
    int wiek_opiekuna; // >25 jeśli petent <18, wedle tych przeklętych wymagań
} Komunikat;

// Deklaracje funkcji pomocniczych 
void log_to_file(const char *msg); 
void sem_p(int semid, int sem_num); 
void sem_v(int semid, int sem_num); 
void sem_op(int semid, int sem_num, int op);
#endif