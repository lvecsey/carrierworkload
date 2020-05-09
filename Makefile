
CC=gcc

CFLAGS=-O3 -Wall -g -pg

LDFLAGS=

LIBS=-lpthread

all : carrierworkload

carrierworkload : carrierworkload.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)
