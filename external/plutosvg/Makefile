include ./make.config

OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/plutosvg

BASE_DIR = $(shell pwd)
PLUTOSVG_DIR = $(BASE_DIR)/plutosvg/

make_dirs:
	-mkdir $(OBJ_DIR)

install-headers: make_dirs

install: 
	set -e; \
	cd $(OBJ_DIR); \
	LDFLAGS="" CFLAGS="" meson setup --cross-file=$(PROJECT_ROOT)/libc/ethereal_cross.txt -Dprefix=$(PREFIX) $(PLUTOSVG_DIR); \
	DESTDIR=$(DESTDIR) ninja install; \
	cd $(BASE_DIR)

clean:
	-rm -rf $(OBJ_DIR)
