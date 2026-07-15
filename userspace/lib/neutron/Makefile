# HACK: Neutron's makefile automatically appends Neutron to this

include ../make.config

OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/userspace/

install:
	-mkdir $(OBJ_DIR)
	OBJ_DIR=$(OBJ_DIR) make -C neutron -f Makefile.ethereal all install
	cp neutron.pc $(DESTDIR)$(LIBDIR)/pkgconfig/
	OBJ_DIR=$(OBJ_DIR) make -C neutron -f Makefile.ethereal DEMO

clean:
	-make -C neutron -f Makefile.ethereal clean
	-rm neutron/demo/demo
