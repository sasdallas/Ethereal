# Base makefile
include ../make.config

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"


# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Creating base, please wait... ]"
	@echo
	@echo


install:
	@echo "Ignored"

install-headers:
	-mkdir -pv $(SYSROOT)
	-mkdir -pv $(INITRD)
	-cp -rp initrd/* $(INITRD)/
	-cp -rp sysroot/* $(SYSROOT)/

	bash -c "$(BUILDSCRIPTS_ROOT)/create-base-structure.sh"

clean:
	-rm -rf $(INITRD)/
	-rm -rf $(SYSROOT)/