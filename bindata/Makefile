include ../CONFIG

CFLAGS+=-I..

all: descramble_map_forward.o descramble_map_reverse.o

descramble_map_forward.o: descramble_map_forward.c
descramble_map_reverse.o: descramble_map_reverse.c

descramble_map_forward.c: descramble_map_forward.bin 
	xxd -i descramble_map_forward.bin > descramble_map_forward.c
descramble_map_reverse.c: descramble_map_reverse.bin
	xxd -i descramble_map_reverse.bin > descramble_map_reverse.c

.PHONY : clean
clean:
	rm -f *.o *.c

