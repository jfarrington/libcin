# Variables for compelation
RM=rm
CC=gcc
CFLAGS=-c -Wall -O3 -D_DESCRAMBLE_ -D_TIFF_OP_
LIBS=
OBJS=$(SRCSi:.c=.o)
MAKE=make
SUBDIRS=tests utils

.PHONY : clean subdirs $(SUBDIRS)

all: cindump libcin cintest subdirs

# create dynamically and statically-linked libs.
# the latter is used to create the "cintest" executable
libcin: cin_api.c cin_api.h cin_register_map.h
	$(CC) -Wall -c -o $@.o $<
	$(AR) -rcs $@.a $@.o
	$(CC) -Wall -fpic -shared -o $@.so $<

cintest: cin_test.c cin_api.c
	$(CC) -Wall -o $@ $^

cindump: cindata.o cindump.o cindata.h bpfilter.h descramble_block.h
	$(CC) cindata.o cindump.o -ltiff -lpthread -o cindump

subdirs: $(SUBDIRS)

$(SUBDIRS): 
	$(MAKE) -C $@

clean:
	rm -f *.o
	rm -f cindump
	rm -f cintest
	rm -f *.so *.a
	$(MAKE) -C tests clean
