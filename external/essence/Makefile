include ./make.config

BASE_DIR = $(shell pwd)

install-headers:
	-mkdir $(DESTDIR)/root/
	-mkdir $(DESTDIR)/bin/

install:
	make -C essence CC=$(CC) LD=$(LD)
	cp essence/build/essence $(DESTDIR)$(PREFIX)/bin/
	cp essence/.esrc $(DESTDIR)/root/
	ln -sf /usr/bin/essence $(DESTDIR)/bin/sh

clean:
	make -C essence clean
