CFLAGS=-ggdb -Wall
LDFLAGS=-pthread -ldl

bdwgc/.libs/libgc.a:
	cd bdwgc
	ln -s ../libatomic_ops
	autoreconf -vif
	automake --add-missing
	CFLAGS="-DPOINTER-MASK=0x0000FFFFFFFFFFFF" ./configure
	make -j8

crisp: bdwgc/.libs/libgc.a crisp.o
	$(CC) $(LIBFLAGS) $(OBJECTS) $(LDFLAGS) crisp.o bdwgc/.libs/libgc.a -o crisp
