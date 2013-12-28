# Variables for compelation
RM=rm
CC=gcc
CFLAGS=-c -Wall -O3
LIBS=
OBJS=$(SRCSi:.c=.o)

.PHONY : clean

all: cindump descramble_test

cindump: listen.o cindump.o fifo.o descramble.o listen.h cindump.h fifo.h descramble.h
	$(CC) listen.o cindump.o fifo.o descramble.o -lpthread -o cindump

descramble_test: descramble_test.o
	$(CC) descramble_test.o -o descramble_test

clean:
	rm -f *.o
	rm -f cindump

