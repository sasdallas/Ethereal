# Userspace Makefile for Hexahedron
include ./make.config

# List of apps to compile
APPS = init miniutils essence dhcpcli celestial wmtest terminal termemu desktop etherealver font-viewer show-dialog tray
APPS += toast-server colorwave plasma memvisualize login-gui

APPS_TARGETS_INSTALL := $(patsubst %, install?%, $(APPS))
APPS_TARGETS_CLEAN := $(patsubst %, clean?%, $(APPS))

USERSPACE_LIB_DIRECTORY = lib

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"

install-lib:
	$(MAKE) -C $(USERSPACE_LIB_DIRECTORY) install

clean-lib:
	$(MAKE) -C $(USERSPACE_LIB_DIRECTORY) clean

install?%:
	$(MAKE) -C $(subst $e?,/,$(patsubst install?%, %, $@)) install

clean?%:
	$(MAKE) -C $(subst $e?,/,$(patsubst clean?%, %, $@)) clean

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

install: PRINT_HEADER MAKE_DIRS install-lib $(APPS_TARGETS_INSTALL)

install-headers:
	make -C $(USERSPACE_LIB_DIRECTORY) install-headers

clean: clean-lib $(APPS_TARGETS_CLEAN)