include ../CONFIG

CFLAGS+=-I../include -I../bindata

.PHONY : clean 

all: cindata.o fifo.o mbuffer.o

cindata.o: cindata.h ../bindata/descramble_map_forward.o ../include/cin.h

fifo.o: fifo.h 

mbuffer.o: mbuffer.h

clean:
	rm -f *.o
