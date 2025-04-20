# Userspace Makefile for Hexahedron
include ./make.config

# List of apps to compile
APPS = init miniutils

APPS_TARGETS_INSTALL := $(patsubst %, install-%, $(APPS))
APPS_TARGETS_CLEAN := $(patsubst %, clean-%, $(APPS))

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
	-@mkdir -pv $(OBJ_OUTPUT_DIRECTORY)/userspace

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building userspace applications, please wait... ]"
	@echo
	@echo

install: PRINT_HEADER MAKE_DIRS $(APPS_TARGETS_INSTALL)

clean: $(APPS_TARGETS_CLEAN)