include ../CONFIG

CFLAGS+=-I..

binfiles = descramble_map_forward.bin descramble_map_reverse.bin
cfiles   = $(binfiles:.bin=.c)
objects  = $(cfiles:.c=.o)

all: bin2c $(objects)
	echo $(objects) $(cfiles) $(names)

bin2c : bin2c.c
	$(CC) bin2c.c -o bin2c

$(cfiles) : $(binfiles)
	./bin2c $< $* > $@

$(objects) : $(cfiles)
	$(CC) -c $< -o $@

.PHONY : clean
clean:
	rm -f *.o 
	rm -f $(cfiles)
	rm -f bin2c
