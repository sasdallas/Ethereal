# Userspace tray Makefile for Hexahedron
include ./make.config

# List of tray widgets to compile
WIDGETS = clock network

WIDGETS_TARGETS_INSTALL := $(patsubst %, install-%, $(WIDGETS))
WIDGETS_TARGETS_CLEAN := $(patsubst %, clean-%, $(WIDGETS))
WIDGETS_DEST_DIR := $(DESTDIR)$(LIBDIR)/widgets/

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"


# !!!: HACK to remove -MD
REMOVE_MD:
	-rm $(WIDGETS_DEST_DIR)/*.d

install-%:
	$(CC) -shared -fPIC -I$(DESTDIR)/usr/include/freetype2 -I$(DESTDIR)/usr/include/ $(wildcard $(subst $e-,/,$(patsubst install-%,%,$@))/*.c) -o $(WIDGETS_DEST_DIR)/$(subst $e-,/,$(patsubst install-%,%,$@)).so -lgraphics -lfreetype

clean-%:
	rm $(WIDGETS_DEST_DIR)/$(subst $e-,/,$(patsubst clean-%,%,$@)).so

# Create build directories
MAKE_DIRS:
	-@mkdir -pv $(WIDGETS_DEST_DIR)

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building userspace tray widgets, please wait... ]"

install: PRINT_HEADER MAKE_DIRS $(WIDGETS_TARGETS_INSTALL) REMOVE_MD
clean: $(WIDGETS_TARGETS_CLEAN)

install-headers:
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	@-cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDE_DIRECTORY)/