CFLAGS+=-I..

.PHONY : clean 

all: cin_api.o

# create dynamically and statically-linked libs.
# the latter is used to create the "cintest" executable
cin_api.o: cin_api.c cin_api.h ../cin.h

clean:
	rm -f *.o
