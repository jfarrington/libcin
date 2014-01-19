include ../CONFIG

CFLAGS+=-I.. -I../bindata

.PHONY : clean 

all: cindata.o fifo.o dbl_buffer.o

cindata.o: cindata.h descramble_map_forward.o ../cin.h

fifo.o: fifo.h 

dbl_buffer.o: dbl_buffer.h

clean:
	rm -f *.o
