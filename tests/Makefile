include ../CONFIG

LDFLAGS+=-L../lib
LDLIBS+=-lcin
CFLAGS+=-I..

.PHONY : clean

all: data_server make_descramble_map cin_test 

data_server: data_server.o data_server.h \
             ../bindata/descramble_map_forward.o

make_descramble_map: make_descramble_map.o

cin_test: cin_test.o ../lib/libcin.a

.PHONY : maps
maps: descramble_map_forward.bin descramble_map_reverse.bin
	xxd -i descramble_map_forward.bin > descramble_map_forward.c
	xxd -i descramble_map_reverse.bin > descramble_map_reverse.c

clean:
	rm -f *.o
	rm -f data_server
	rm -f make_descramble_map


