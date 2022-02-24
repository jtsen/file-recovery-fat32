CC=gcc
LDFLAGS= -l crypto
CFLAGS=-g -Wall -pedantic -std=gnu99

.PHONY: all
all: nyufile

nyuenc: nyufile.o

nyuenc.o: nyufile.c nyufile.h

.PHONY: clean
clean:
	rm -f *.o nyufile