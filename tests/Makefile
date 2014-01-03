LDFLAGS+=-L../lib
LDLIBS+=-lcin
CFLAGS+=-I..

.PHONY : clean

all: data_server make_descramble_map cin_test

data_server: data_server.o data_server.h
#	$(CC) -o $@ data_server.o

make_descramble_map: make_descramble_map.o
#	$(CC) -o $@ make_descramble_map.o

cin_test: cin_test.o 
#	$(CC) -o $@ cin_test.o -lcin

clean:
	rm -f *.o
	rm -f data_server
	rm -f make_descramble_map


