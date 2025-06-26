# libpolyhedron Makefile

include ./make.config

OUT_OBJ = $(OBJ_OUTPUT_DIRECTORY)/libpolyhedron

OUT_OBJ_LIBC = $(OUT_OBJ)/libc
OUT_OBJ_LIBK = $(OUT_OBJ)/libk

# SOURCE DIRECTORIES
SOURCE_DIRECTORIES = stdlib stdio string time net
LIBC_SOURCE_DIRECTORIES = $(SOURCE_DIRECTORIES) errno unistd libc dirent mmap signal locale libgen socket poll sys ethereal wchar pthread
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

CRT_SOURCES = $(ARCH_CRT_DIRECTORY)/crtn.S $(ARCH_CRT_DIRECTORY)/crti.S $(ARCH_CRT_DIRECTORY)/crt0.S
CRT_OBJECTS = $(patsubst %.S, $(OUT_OBJ_LIBC)/%.o, $(CRT_SOURCES))
CRT_OUTPUT_DIR = $(addprefix $(OUT_OBJ_LIBC)/,$(ARCH_CRT_DIRECTORY))

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
$(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a: $(LIBC_C_OBJECTS) $(CRT_OBJECTS)
	$(AR) rcs $@ $(LIBC_C_OBJECTS) $(CRT_OBJECTS)
	-ln -sf $@ $(DESTDIR)$(LIB_DIRECTORY)/libc.a
	cp $(OUT_OBJ_LIBC)/arch/$(BUILD_ARCH)/crt/crt0.o $(DESTDIR)$(LIB_DIRECTORY)
	-cp $(OUT_OBJ_LIBC)/arch/$(BUILD_ARCH)/crt/crti.o $(DESTDIR)$(LIB_DIRECTORY)
	-cp $(OUT_OBJ_LIBC)/arch/$(BUILD_ARCH)/crt/crtn.o $(DESTDIR)$(LIB_DIRECTORY)


$(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.so: $(LIBC_C_OBJECTS) | $(CRT_BJECTS)
	$(CC) -nodefaultlibs -shared -fPIC -o $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron_dynamic.so $^ -lgcc

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
	@-mkdir -pv $(CRT_OUTPUT_DIR)

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

install: PRINT_HEADER CREATE_BUILD_DIRECTORIES $(DESTDIR)$(LIB_DIRECTORY)/libkpolyhedron.a $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.so

clean:
	@-rm -r $(OUT_OBJ)
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libkpolyhedron.a
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.a
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libpolyhedron.so
