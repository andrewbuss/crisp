CFLAGS=-ggdb -Wall
LDFLAGS=-pthread -ldl

crisp: bdwgc/.libs/libgc.a crisp.o
	$(CC) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) crisp.o bdwgc/.libs/libgc.a -o crisp

.ONESHELL:
bdwgc/.libs/libgc.a:
	git submodule update --init
	cd bdwgc
	ln -s ../libatomic_ops
	autoreconf -vif
	automake --add-missing
	CFLAGS="-DPOINTER_MASK=0x0000FFFFFFFFFFFF" ./configure
	make -j8

