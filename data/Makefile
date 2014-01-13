include ../CONFIG

CFLAGS+=-I..

.PHONY : clean 

all: cindata.o fifo.o

cindata.o: cindata.h descramble_map_forward.o ../cin.h
fifo.o: fifo.h 

clean:
	rm -f *.o
