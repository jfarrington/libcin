# Variables for compelation
RM=rm
CC=gcc
CFLAGS=-Wall -O3 -D__DESCRAMBLE__ -I.
LDFLAGS=-L.
MAKE=make
SUBDIRS=control tests

LIBHEADERS=control/cin_api.h \
           control/cin_register_map.h
LIBSOURCES=control/cin_api.c
LIBOBJECTS=$(LIBSOURCES:.c=.o)

.PHONY : clean subdirs $(SUBDIRS)

# Export all variables to sub make processes
export

all: subdirs

# create dynamically and statically-linked libs.
libcin: $(LIBOBJECTS) $(LIBSOURCES)
	$(AR) -rcs lib/$@.a $(LIBOBJECTS)
#	$(CC) -Wall -fpic -shared -o $@.so $<

subdirs: $(SUBDIRS)

$(SUBDIRS): 
	$(MAKE) -C $@

clean:
	$(RM) -f *.o
	$(RM) -f lib/*.so lib/*.a
	$(MAKE) -C tests clean
	$(MAKE) -C control clean
