include ../CONFIG

CFLAGS+=-I../include -I../bindata

.PHONY : clean 

all: cindata.o fifo.o mbuffer.o

cindata.o: cindata.h fifo.h mbuffer.h \
           ../include/cin.h \
           ../bindata/descramble_map.h

fifo.o: fifo.h 

mbuffer.o: mbuffer.h

clean:
	rm -f *.o
