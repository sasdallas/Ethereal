include ../make.config

ifeq ($(HOSTARCH), i386)
# Required for I386
PROJECTS := musl freetype

# ACPICA
ifeq ($(USE_ACPICA), 1)
PROJECTS += acpica
endif

else ifeq ($(HOSTARCH), x86_64)
# Required for x86_64
PROJECTS := musl freetype

# ACPICA
ifeq ($(USE_ACPICA), 1)
PROJECTS += acpica
endif

else ifeq ($(HOSTARCH), aarch64)
# Required for aarch64
# Freetype cannot be built
PROJECTS := musl

else
PROJECTS :=
endif





# i love make i love make cmake is bad i love make i love make
PROJECTS_HEADERS := $(addsuffix /installheaders, $(PROJECTS))
PROJECTS_INSTALL := $(addsuffix /install, $(PROJECTS))
PROJECTS_CLEAN := $(addsuffix /clean, $(PROJECTS))

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"

$(PROJECTS_HEADERS):
	make -C $(dir $@) install-headers

$(PROJECTS_INSTALL):
	make -C $(dir $@) install

$(PROJECTS_CLEAN):
	make -C $(dir $@) clean

# Because I could care less each external project should at least have an
# install-headers dummy target. Suck it.
install-headers: $(PROJECTS_HEADERS)
	@echo "[EXTERNAL] Finished headers: $(PROJECTS)"
install: $(PROJECTS_INSTALL)
	@echo "[EXTERNAL] Finished compiling: $(PROJECTS)"
clean: $(PROJECTS_CLEAN)