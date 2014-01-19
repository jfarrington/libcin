include CONFIG

all: control data lib/libcin.a tests utils

# create dynamically and statically-linked libs.
lib/libcin.a: $(LIBOBJECTS) 
	test -d lib || mkdir lib
	$(AR) -rcs $@ $(LIBOBJECTS)

lib/libcin.so:  $(LIBSOURCES)
	test -d lib || mkdir lib
	$(CC) $(CFLAGS) -shared -o $@ $(LIBOBJECTS)

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
