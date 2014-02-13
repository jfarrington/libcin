include CONFIG

all: bindata control data lib/libcin.a tests utils

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
	$(MAKE) -C bindata clean

.PHONY :install
install: all
	test -d $(prefix)         || mkdir $(prefix)
	test -d $(prefix)/lib     || mkdir $(prefix)/lib
	test -d $(prefix)/bin     || mkdir $(prefix)/bin
	test -d $(prefix)/include || mkdir $(prefix)/include
	$(INSTALL_DATA) lib/libcin.a $(libdir)
	$(INSTALL_DATA) cin.h $(includedir)
	$(INSTALL_PROGRAM) utils/cindump $(bindir)

