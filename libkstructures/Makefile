# libstructures Makefile

include ./make.config

# OUT DIRECTORIES
OUT_OBJ_KERNEL = $(OBJ_OUTPUT_DIRECTORY)/libkstructures
OUT_OBJ_LIBC = $(OBJ_OUTPUT_DIRECTORY)/libstructures

# SOURCE DIRECTORIES
SOURCE_DIRECTORIES_KERNEL = list tree json hashmap circbuf
SOURCE_DIRECTORIES_LIBC = list tree json hashmap
OUTPUT_DIRECTORIES_KERNEL = $(addprefix $(OUT_OBJ_KERNEL)/,$(SOURCE_DIRECTORIES_KERNEL))
OUTPUT_DIRECTORIES_LIBC = $(addprefix $(OUT_OBJ_LIBC)/,$(SOURCE_DIRECTORIES_LIBC))

# Construct a basic list of C sources
KERNEL_C_SOURCES = $(shell find $(SOURCE_DIRECTORIES_KERNEL) -maxdepth 1 -name "*.c")
KERNEL_C_OBJECTS = $(patsubst %.c, $(OUT_OBJ_KERNEL)/%.o, $(KERNEL_C_SOURCES))
LIBC_C_SOURCES = $(shell find $(SOURCE_DIRECTORIES_LIBC) -maxdepth 1 -name "*.c")
LIBC_C_OBJECTS = $(patsubst %.c, $(OUT_OBJ_LIBC)/%.o, $(LIBC_C_SOURCES))

# Include the architecture Make configuration to allow it to add to DIRECTORIES
include arch/$(BUILD_ARCH)/make.config

# Compile a C file (kernel)
$(OUT_OBJ_KERNEL)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

# Compile a C file (libc)
$(OUT_OBJ_LIBC)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

# Build target for libkstructures
$(DESTDIR)$(LIB_DIRECTORY)/libkstructures.a: $(KERNEL_C_OBJECTS)
	$(AR) rcs $@ $(KERNEL_C_OBJECTS)

# Build target for libstructures
$(DESTDIR)$(LIB_DIRECTORY)/libstructures.a: $(LIBC_C_OBJECTS)
	$(AR) rcs $@ $(LIBC_C_OBJECTS)

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"

# Makes output directories
CREATE_BUILD_DIRECTORIES:
	@-mkdir -pv $(OUT_OBJ_KERNEL)
	@-mkdir -pv $(OUT_OBJ_LIBC)
	@-mkdir -pv $(OUTPUT_DIRECTORIES_KERNEL)
	@-mkdir -pv $(OUTPUT_DIRECTORIES_LIBC)
	@-mkdir -pv $(DESTDIR)$(LIB_DIRECTORY)

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building libkstructures, please wait... ]"
	@echo
	@echo

install-headers:
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	@-cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDE_DIRECTORY)/

install: PRINT_HEADER CREATE_BUILD_DIRECTORIES $(DESTDIR)$(LIB_DIRECTORY)/libkstructures.a $(DESTDIR)$(LIB_DIRECTORY)/libstructures.a

clean:
	@-rm -r $(OUT_OBJ_KERNEL)
	@-rm -r $(OUT_OBJ_LIBC)
	@-rm -r $(DESTDIR)$(LIB_DIRECTORY)/libkstructures.a