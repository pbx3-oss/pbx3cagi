CC=gcc
CFLAGS=-g -O2 -Wall

all:
	make cagisample fastcagi

cagisample: cagi.o  cagisample.o
	${CC} -o cagisample cagi.o cagisample.o

fastcagi: fastcagi.o
	${CC} -o fastcagi fastcagi.o

clean:
	rm -f cagisample fastcagi
	rm -f *.o

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

