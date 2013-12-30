# Variables for compelation
RM=rm
CC=gcc
CFLAGS=-c -Wall -O3
LIBS=
OBJS=$(SRCSi:.c=.o)

.PHONY : clean

all: cindump descramble_test libcin cintest

# create dynamically and statically-linked libs.
# the latter is used to create the "cintest" executable
libcin: cin_api.c cin_api.h cin_register_map.h
	$(CC) -Wall -c -o $@.o $<
	$(AR) -rcs $@.a $@.o
	$(CC) -Wall -fpic -shared -o $@.so $<

cintest: cin_test.c cin_api.c
	$(CC) -Wall -o $@ $^

cindump: cindata.o cindump.o cindata.h bpfilter.h descramble_block.h
	$(CC) cindata.o cindump.o -lpthread -o cindump

descramble_test: descramble_test.o
	$(CC) descramble_test.o -o descramble_test

clean:
	rm -f *.o
	rm -f cindump
	rm -f descramble_test
	rm -f cintest
	rm -f *.so *.a

