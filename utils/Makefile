LDFLAGS+=-L../lib
LDLIBS+=-lcin -ltiff
CFLAGS+=-I..

.PHONY : clean

all: cindump

cin_test: cindump.o ../cin.h ../libcin.a

clean:
	rm -f *.o
	rm -f cindump


