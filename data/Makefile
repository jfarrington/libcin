include ../CONFIG

CFLAGS+=-I.. -I../bindata

.PHONY : clean 

all: cindata.o fifo.o mbuffer.o

cindata.o: cindata.h descramble_map_forward.o ../cin.h

fifo.o: fifo.h 

mbuffer.o: mbuffer.h

clean:
	rm -f *.o
