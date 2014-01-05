include CONFIG

all: control data libcin tests utils

# create dynamically and statically-linked libs.
libcin: 
	$(AR) -rcs lib/$@.a $(LIBOBJECTS)
#	$(CC) $(CFLAGS) -fpic -shared -I. -o lib/$@.so $(LIBSOURCES)

$(SUBDIRS): 
	$(MAKE) -C $@

.PHONY :clean
clean:
	-$(RM) -f *.o
	-$(RM) -f lib/libcin.so lib/libcin.a
	$(MAKE) -C data clean
	$(MAKE) -C tests clean
	$(MAKE) -C control clean
	$(MAKE) -C utils clean
