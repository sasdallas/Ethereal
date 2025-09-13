# mlibc Makefile integration

include ./make.config

OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/mlibc
OBJ_DIR2 = $(OBJ_OUTPUT_DIRECTORY)/mlibc_headers
LIBC_DIR=$(shell pwd)
MLIBC_DIR = $(LIBC_DIR)/mlibc

$(info $(MLIBC_DIR))

make_dirs:
	-mkdir $(OBJ_DIR)
	-mkdir $(OBJ_DIR2)

install-headers: make_dirs
	# Install linux headers
	bash ./linux_install_headers.sh

	# Install mlibc headers
	if ! [ -d mlibc ]; then \
		echo -e "\n\nYou forgot to run git submodule init and git submodule update\n\n"; \
		exit 1; \
	fi

	cd $(OBJ_DIR2); \
	meson setup --cross-file=$(LIBC_DIR)/ethereal_cross.txt -Dheaders_only=true -Dprefix=$(PREFIX) -Ddefault_library=both -Dlinux_kernel_headers=$(DESTDIR)/usr/include/ $(MLIBC_DIR); \
	ninja install; \
	cd $(LIBC_DIR); 

	cat $(DESTDIR)/usr/include/limits.h

	x86_64-ethereal-gcc -c limits-test.c -o limits-test.o
	rm limits-test.o

install:
	# Install mlibc
	set -e; \
	cd $(OBJ_DIR); \
	meson setup --cross-file=$(LIBC_DIR)/ethereal_cross.txt -Dheaders_only=false -Dprefix=$(PREFIX) -Ddefault_library=shared -Dlinux_kernel_headers=$(DESTDIR)/usr/include/  --reconfigure $(MLIBC_DIR); \
	ninja install; \
	cd $(LIBC_DIR)

clean:
	-rm -rf $(OBJ_DIR)
	-rm -rf linux-6.16