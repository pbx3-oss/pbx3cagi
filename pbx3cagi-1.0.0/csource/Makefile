CC=gcc
CFLAGS=-c -W -std=c99
# CPFLAGS for C99
#CPPFLAGS=-std=c99
#DFLAGS=-D DEBUG
DFLAGS=

# macOS/FreeBSD have strlcpy/strlcat in libc; Linux needs libbsd
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LDFLAGS_BSD :=
else
  ifeq ($(UNAME_S),FreeBSD)
    LDFLAGS_BSD :=
  else
    LDFLAGS_BSD := -lbsd
  endif
endif

all: pbx3cagi

pbx3cagi:  pbx3cagi.o agi_sqlite.o agi_wrap.o cagi.o sqlite3.o
	$(CC) pbx3cagi.o agi_sqlite.o agi_wrap.o cagi.o sqlite3.o -o pbx3cagi -pthread -ldl $(LDFLAGS_BSD)

pbx3cagi.o: pbx3cagi.c
	$(CC) $(CFLAGS) pbx3cagi.c $(DFLAGS) 

agi_sqlite.o: agi_sqlite.c
	$(CC) $(CFLAGS) agi_sqlite.c $(DFLAGS)

agi_wrap.o: agi_wrap.c
	$(CC) $(CFLAGS) agi_wrap.c $(DFLAGS)

cagi.o: cagi.c
	$(CC) $(CFLAGS) cagi.c

sqlite3.o: sqlite3.c
	$(CC) $(CFLAGS) sqlite3.c -Os

pbx3cagi.c: pbx3cagi.h agi_sqlite.h agi_wrap.h

agi_sqlite.c: agi_sqlite.h pbx3cagi.h

agi_wrap.c: agi_wrap.h pbx3cagi.h cagi.h

cagi.c: cagi.h

clean:
	rm -rf *o pbx3cagi

test:
	$(MAKE) all
	../tests/run-all-scenarios.sh


