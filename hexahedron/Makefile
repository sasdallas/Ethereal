# Hexahedron Makefile
include ./make.config

# Output folders
OUT_OBJ = $(OBJ_OUTPUT_DIRECTORY)/hexahedron/
OUT_BUILD = $(BUILD_OUTPUT_DIRECTORY)/hexahedron

# Edit this to add more source directories. MAKE SURE TO HAVE SUBDIRECTORIES SEPARATE!
SOURCE_DIRECTORIES = kernel kernel/panic drivers drivers/usb drivers/usb/hid drivers/net drivers/sound drivers/storage debug mem misc gfx fs loader task klib/string klib/stdio klib/stdlib
 
# Edit this to change the allocator in use. ONLY ONE.
SOURCE_DIRECTORIES += mem/liballoc/

# Include the architecture Make configuration
include arch/$(BUILD_ARCH)/make.config

# NOW INCLUDE THE REST! LIBRARY ORDER MATTERS!!!
# -lkstructures must not use anything from before it
# -lkpolyhedron must not use anything from libkstructures
# -lgcc must not use anything from libkpolyhedron
LIBS += -lkstructures -lgcc

# Construct object output directories
OUTPUT_DIRECTORIES = $(addprefix $(OUT_OBJ)/,$(SOURCE_DIRECTORIES))

# Compile a list of C sources and C assembly sources
C_SOURCES = $(shell find $(SOURCE_DIRECTORIES) -maxdepth 1 -name "*.c" -type f)
C_ASM_SOURCES = $(shell find $(SOURCE_DIRECTORIES) -maxdepth 1 -name "*.S" -type f)

# Construct object lists
C_OBJECTS = $(patsubst %.c, $(OUT_OBJ)/%.o, $(C_SOURCES)) $(patsubst %.S, $(OUT_OBJ)/%.o, $(C_ASM_SOURCES))

# Target disabled
all:
	@echo "=== TARGET DISABLED ==="
	@echo "Please use make install"


# Compilation functions
$(OUT_OBJ)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_OBJ)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@


# Linker
$(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf: $(C_OBJECTS)
	@printf "\n-- Linking Hexahedron kernel...\n"
	$(LD) $(LDFLAGS) -T $(ARCH_LINK_LD) $(C_OBJECTS) -o $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf $(LIBS)

	@printf "\n-- Copying debug symbols...\n"
	$(OBJCOPY) --only-keep-debug $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel-symbols.sym
	$(OBJCOPY) --strip-debug $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf

	@printf "\n-- Creating symbol map...\n"
	$(NM) $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf > $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel-symmap.map
	cp $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel-symmap.map $(INITRD)/

	$(PYTHON) $(BUILDSCRIPTS_ROOT)/create_bochs_symfile.py $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel-symmap.map $(DESTDIR)$(BOOT_OUTPUT)/bochs.map 


ifneq ($(HOSTARCH), aarch64)
	@printf "\n-- Checking multiboot compatibility (make sure you have the GRUB package)...\n"
	grub-file --is-x86-multiboot $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf
	grub-file --is-x86-multiboot2 $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf
endif

# Create build directories
CREATE_BUILD_DIRECTORIES:
	@-mkdir -pv $(OUT_OBJ)
	@-mkdir -pv $(OUTPUT_DIRECTORIES)
	@-mkdir -pv $(DESTDIR)$(BOOT_OUTPUT)

# Cosmetic
PRINT_HEADER:
	@echo
	@echo
	@echo "[ Building Hexahedron, please wait... ]"
	@echo
	@echo

# Recompilation
DELETE_OBJECTS:
	@-rm $(OUT_OBJ)/kernel/config.o 
	@-rm $(OUT_OBJ)/kernel/panic/panic_messages.o

# Main kernel build targets
install-headers:
	@echo "-- Installing Hexahedron headers..."
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	cp -rp include/kernel/ $(DESTDIR)$(INCLUDE_DIRECTORY)

ifeq ($(HOSTARCH), aarch64)

bootstub:
	make -C arch/aarch64/bootstubs install

install: PRINT_HEADER CREATE_BUILD_DIRECTORIES DELETE_OBJECTS $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf bootstub
else
install: PRINT_HEADER CREATE_BUILD_DIRECTORIES DELETE_OBJECTS $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf
endif

clean:
	@-rm -r $(OUT_OBJ)
	@-rm -r $(DESTDIR)$(BOOT_OUTPUT)/hexahedron-kernel.elf

ifeq ($(HOSTARCH), aarch64)
	make -C arch/aarch64/bootstubs clean
endif