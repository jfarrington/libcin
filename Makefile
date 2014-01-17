include CONFIG

all: control data libcin tests utils

# create dynamically and statically-linked libs.
libcin: 
	test -d lib || mkdir lib
	$(AR) -rcs lib/$@.a $(LIBOBJECTS)
#	$(CC) $(CFLAGS) -fpic -shared -I. -o lib/$@.so $(LIBSOURCES)

$(SUBDIRS): 
	$(MAKE) -C $@

.PHONY :clean
clean:
	-$(RM) -f *.o
	-$(RM) -rf lib
	$(MAKE) -C data clean
	$(MAKE) -C tests clean
	$(MAKE) -C control clean
	$(MAKE) -C utils clean
