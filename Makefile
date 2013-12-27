# Variables for compelation
RM=rm
CC=gcc
CFLAGS=-c -Wall 
LIBS=
OBJS=$(SRCSi:.c=.o)

.PHONY : clean

all: cindump

cindump: listen.o cindump.o fifo.o descramble.o listen.h cindump.h fifo.h descramble.h
	$(CC) listen.o cindump.o fifo.o descramble.o -lpthread -o cindump

clean:
	rm -f *.o
	rm -f cindump

