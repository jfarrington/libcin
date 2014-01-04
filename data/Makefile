CFLAGS+=-I..

.PHONY : clean 

all: cindata.o fifo.o

cindata.o: cindata.h descramble_block.h ../cin.h
fifo.o: fifo.h 

clean:
	rm -f *.o
