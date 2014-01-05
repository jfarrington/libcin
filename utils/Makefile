include ../CONFIG

LDFLAGS+=-L../lib 
LDLIBS+=-lcin -ltiff -llz4
CFLAGS+=-I..

.PHONY : clean

all: cindump

cindump: cindump.o ../lib/libcin.a

clean:
	rm -f *.o
	rm -f cindump


