all: libmigdb.a

libmigdb.a: mi_gdb.h
	cd libmigdb/src/ && $(MAKE)
	mv libmigdb/src/libmigdb.a .

clean:
	rm -rf libmigdb.a
	cd libmigdb/src && $(MAKE) clean
