# i386 build configuration for Hexahedron/Polyhedron/any I386-based project
# YOU MAY EDIT THIS

# ACPICA settings
USE_ACPICA = 1

ifeq ($(USE_ACPICA), 1)
CFLAGS += -DACPICA_ENABLED
endif

# !!!: HACK
CFLAGS += -I$(PROJECT_ROOT)/build-output/sysroot/usr/include/freetype2/