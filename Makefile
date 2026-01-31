CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -pthread -g
LDFLAGS = -pthread
NCURSES_FLAGS = -lncurses

BINARIES = dyrektor petent urzednik rejestracja monitor

DYREKTOR_OBJ = main.o common.o
PETENT_OBJ   = petent.o common.o
URZEDNIK_OBJ = urzednik.o common.o
REJESTR_OBJ  = rejestracja.o common.o
MONITOR_OBJ  = monitor.o common.o

all: $(BINARIES)

dyrektor: $(DYREKTOR_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

petent: $(PETENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

urzednik: $(URZEDNIK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

rejestracja: $(REJESTR_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

monitor: $(MONITOR_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(NCURSES_FLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $<

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

clean:
	rm -f *.o $(BINARIES)

.PHONY: all clean