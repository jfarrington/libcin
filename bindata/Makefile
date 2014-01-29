include ../CONFIG

CFLAGS+=-I..

binfiles = descramble_map_forward.bin
cfiles   = $(binfiles:.bin=.c)
objects  = $(cfiles:.c=.o)
names    = $(cfiles:.c= )

all: bin2c $(objects)
	echo $(objects) $(cfiles) $(names)

bin2c : bin2c.c
	$(CC) bin2c.c -o bin2c

$(cfiles) : $(binfiles)
	./bin2c $< name > $@

$(objects) : $(cfiles)
	$(CC) -c $< -o $@

.PHONY : clean
clean:
	rm -f *.o 
	rm -f descramble_map_forward.c
	rm -f descramble_map_reverse.c
	rm -f bin2c
