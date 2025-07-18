# x86_64 build configuration for Hexahedron/Polyhedron/any x86_64-based project
# YOU MAY EDIT THIS

# ACPICA settings
USE_ACPICA = 1

ifeq ($(USE_ACPICA), 1)
CFLAGS += -DACPICA_ENABLED
endif

CFLAGS += -mcmodel=large

# !!!: HACK
CFLAGS += -I$(PROJECT_ROOT)/build-output/sysroot/usr/include/freetype2/