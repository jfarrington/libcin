include ../CONFIG

LDFLAGS+=-L../lib 
LDLIBS+=-lcin -ltiff
CFLAGS+=-I..

#For testing compression uncomment
#LDLIBS+=-llz4
#CFLAGS+=-D__COMPRESS__

.PHONY : clean

all: cindump

cindump: cindump.o ../lib/libcin.a

clean:
	rm -f *.o
	rm -f cindump


