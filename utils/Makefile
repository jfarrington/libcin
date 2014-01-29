include ../CONFIG

LDFLAGS+=-L../lib 
LDLIBS+=-ltiff -lcin
CFLAGS+=-I..

.PHONY : clean

all: cindump.o cindump

cindump.o: cindump.c

cindump: cindump.o ../lib/libcin.a
	$(CC) $(LDFLAGS) cindump.o -o $@ $(LDLIBS) 

clean:
	rm -f *.o
	rm -f cindump


