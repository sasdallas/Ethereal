# Bootstub Makefile for Hexahedron
include ../make.config

# List of bootstubs to compile
BOOTSTUBS = qemu_virt

BOOTSTUBS_TARGETS_INSTALL := $(patsubst %, install-%, $(BOOTSTUBS))
BOOTSTUBS_TARGETS_CLEAN := $(patsubst %, clean-%, $(BOOTSTUBS))


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
	-@mkdir -pv $(OBJ_OUTPUT_DIRECTORY)/bootstubs/

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building AARCH64 bootstubs, please wait... ]"
	@echo
	@echo

install: PRINT_HEADER MAKE_DIRS  $(BOOTSTUBS_TARGETS_INSTALL)

clean: $(BOOTSTUBS_TARGETS_CLEAN)