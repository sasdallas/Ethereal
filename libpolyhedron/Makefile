# libpolyhedron Makefile

include ./make.config

OUT_OBJ = $(OBJ_OUTPUT_DIRECTORY)/libpolyhedron

OUT_OBJ_LIBC = $(OUT_OBJ)/libc
OUT_OBJ_LIBK = $(OUT_OBJ)/libk

# SOURCE DIRECTORIES
SOURCE_DIRECTORIES = stdlib stdio string time net
LIBC_SOURCE_DIRECTORIES = $(SOURCE_DIRECTORIES) errno unistd libc dirent
LIBK_SOURCE_DIRECTORIES = $(SOURCE_DIRECTORIES)

# Include the architecture Make configuration to allow it to add to DIRECTORIES
include arch/$(BUILD_ARCH)/make.config

# Construct our output directories
OUTPUT_DIRECTORIES_LIBC = $(addprefix $(OUT_OBJ_LIBC)/,$(LIBC_SOURCE_DIRECTORIES))
OUTPUT_DIRECTORIES_LIBK = $(addprefix $(OUT_OBJ_LIBK)/,$(LIBK_SOURCE_DIRECTORIES))

# Construct a basic list of C sources
LIBC_C_SOURCES = $(shell find $(LIBC_SOURCE_DIRECTORIES) -name "*.c")
LIBK_C_SOURCES = $(shell find $(LIBK_SOURCE_DIRECTORIES) -name "*.c")

LIBC_ASM_SOURCES = $(shell find $(LIBC_SOURCE_DIRECTORIES) -name "*.S")
LIBK_ASM_SOURCES = $(shell find $(LIBK_SOURCE_DIRECTORIES) -name "*.S")

LIBC_C_OBJECTS = $(patsubst %.c, $(OUT_OBJ_LIBC)/%.o, $(LIBC_C_SOURCES)) $(patsubst %.S, $(OUT_OBJ_LIBC)/%.o, $(LIBC_ASM_SOURCES))
LIBK_C_OBJECTS = $(patsubst %.c, $(OUT_OBJ_LIBK)/%.o, $(LIBK_C_SOURCES))


$(OUT_OBJ_LIBC)/%.o: %.c Makefile
	$(CC) $(LIBC_CFLAGS) -c $< -o $@

$(OUT_OBJ_LIBC)/%.o: %.S Makefile
	$(CC) $(LIBC_CFLAGS) -c $< -o $@

$(OUT_OBJ_LIBK)/%.o: %.c Makefile
	$(CC) $(LIBK_CFLAGS) -c $< -o $@

$(OUT_OBJ_LIBK)/%.o: %.S Makefile
	$(CC) $(LIBK_CFLAGS) -c $< -o $@

# Build target for libk (TODO: bad naming convention)
$(DESTDIR)$(LIB_DIRECTORY)/libkpolyhedron.a: $(LIBK_C_OBJECTS)
	$(AR) rcs $@ $(LIBK_C_OBJECTS)

# Build target for libc 
$(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a: $(LIBC_C_OBJECTS)
	$(AR) rcs $@ $(LIBC_C_OBJECTS)


# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"

# Makes output directories
CREATE_BUILD_DIRECTORIES:
	@-mkdir -pv $(OUT_OBJ)
	@-mkdir -pv $(OUT_OBJ_LIBC)
	@-mkdir -pv $(OUT_OBJ_LIBK)
	@-mkdir -pv $(OUTPUT_DIRECTORIES_LIBC)
	@-mkdir -pv $(OUTPUT_DIRECTORIES_LIBK)
	@-mkdir -pv $(DESTDIR)$(LIB_DIRECTORY)

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building Polyhedron, please wait... ]"
	@echo
	@echo

install-headers:
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	@-cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDE_DIRECTORY)/
	@-cp -R --preserve=timestamps $(ARCH_INCLUDE_DIRECTORY)/. $(DESTDIR)$(INCLUDE_DIRECTORY)/

install: PRINT_HEADER CREATE_BUILD_DIRECTORIES $(DESTDIR)$(LIB_DIRECTORY)/libkpolyhedron.a $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a

clean:
	@-rm -r $(OUT_OBJ)
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libkpolyhedron.a
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a