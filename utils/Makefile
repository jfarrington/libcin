LDFLAGS+=-L../lib
LDLIBS+=-lcin -ltiff
CFLAGS+=-I..

.PHONY : clean

all: cindump

cin_test: cindump.o

clean:
	rm -f *.o
	rm -f cindump


