CC=gcc
CFLAGS=-c -W -lbsd -std=c99
# CPFLAGS for C99
#CPPFLAGS=-std=c99
#DFLAGS=-D DEBUG
DFLAGS=

all: pbx3cagi

pbx3cagi:  pbx3cagi.o cagi.o sqlite3.o
	$(CC) pbx3cagi.o cagi.o sqlite3.o -o pbx3cagi -pthread -ldl -lbsd

pbx3cagi.o: pbx3cagi.c
	$(CC) $(CFLAGS) pbx3cagi.c $(DFLAGS) 

cagi.o: cagi.c
	$(CC) $(CFLAGS) cagi.c

sqlite3.o: sqlite3.c
	$(CC) $(CFLAGS) sqlite3.c -Os

pbx3cagi.c: pbx3cagi.h

cagi.c: cagi.h

clean:
	rm -rf *o pbx3cagi


