# Variables for compelation
CC=gcc
CFLAGS=-c -Wall -O3 
LIBS=
OBJS=$(SRCSi:.c=.o)

.PHONY : clean

all: data_server make_descramble_map

data_server: data_server.o data_server.h
	$(CC) -o $@ data_server.o

make_descramble_map: make_descramble_map.o
	$(CC) -o $@ make_descramble_map.o

clean:
	rm -f *.o
	rm -f data_server
	rm -f make_descramble_map


