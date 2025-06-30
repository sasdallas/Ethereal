# Userspace libraries Makefile for Hexahedron
include ./make.config

# List of libraries to compile
LIBS = graphics celestial keyboard

LIBS_TARGETS_INSTALL := $(patsubst %, install-%, $(LIBS))
LIBS_TARGETS_CLEAN := $(patsubst %, clean-%, $(LIBS))

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"


install-%:
	$(MAKE) -C $(subst $e-,/,$(patsubst install-%, %, $@)) install

clean-%:
	$(MAKE) -C $(subst $e-,/,$(patsubst clean-%, %, $@)) clean

# Create build directories
MAKE_DIRS:
	-@mkdir -pv $(DESTDIR)$(BINARY_DIR)
	-@mkdir -pv $(OBJ_OUTPUT_DIRECTORY)/userspace/lib/

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building userspace libraries, please wait... ]"
	@echo
	@echo

install: PRINT_HEADER MAKE_DIRS $(LIBS_TARGETS_INSTALL)
clean: $(LIBS_TARGETS_CLEAN)

install-headers:
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	@-cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDE_DIRECTORY)/