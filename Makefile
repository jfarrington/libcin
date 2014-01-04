# Variables for compelation
CC=gcc
CFLAGS=-Wall -O3 -D__DESCRAMBLE__ 
LDFLAGS=-L.
LDLIBS=-lpthread

SUBDIRS=control data tests utils

LIBHEADERS=control/cin_register_map.h \
           data/descramble_block.h \
           cin.h

LIBSOURCES=control/cin_api.c data/cindata.c data/fifo.c

LIBOBJECTS=$(LIBSOURCES:.c=.o)

.PHONY : clean subdirs $(SUBDIRS)

# Export all variables to sub make processes
export

all: control data libcin tests utils

# create dynamically and statically-linked libs.
libcin: $(LIBOBJECTS) $(LIBSOURCES) $(LIBHEADERS)
	$(AR) -rcs lib/$@.a $(LIBOBJECTS)
#$(CC) $(CFLAGS) -fpic -shared -o lib/$@.so $(LIBSOURCES)

$(SUBDIRS): 
	$(MAKE) -C $@

clean:
	$(RM) -f *.o
	$(RM) -f lib/*.so lib/*.a
	$(MAKE) -C tests clean
	$(MAKE) -C control clean
	$(MAKE) -C utils clean
