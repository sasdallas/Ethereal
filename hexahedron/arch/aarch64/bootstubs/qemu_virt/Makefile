# bootstub makefile

include ../make.config

OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/bootstubs/$(shell basename $(CURDIR))

C_SRCS = $(shell find . -name "*.c" -printf '%f ')
ASM_SRCS = $(filter-out boot.S,$(shell find . -name "*.S" -printf '%f '))
C_OBJS = boot.o $(patsubst %.S, $(OBJ_DIR)/%.o, $(ASM_SRCS)) $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SRCS))

$(info $(C_OBJS))

OUTPUT_FILE = $(shell basename $(CURDIR)).elf

PRINT_HEADER:
	@echo "-- Building bootstub \"$(OUTPUT_FILE)\""

MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)
	-@mkdir -p $(DESTDIR)$(BOOT_OUTPUT)

$(OBJ_DIR)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@


boot.o: boot.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

./$(OUTPUT_FILE): $(C_OBJS)
	$(LD) -T linker.ld -nostdlib -o $(OUTPUT_FILE) $(C_OBJS)
	$(OBJCOPY) --strip-debug $(OUTPUT_FILE)


install: PRINT_HEADER MAKE_OUTPUT ./$(OUTPUT_FILE)
	cp -r $(OUTPUT_FILE) $(DESTDIR)$(BOOT_OUTPUT)/
	rm ./$(OUTPUT_FILE)

clean: 
	-rm ./$(OUTPUT_FILE)
	-rm -rf $(DESTDIR)$(BOOT_OUTPUT)/$(OUTPUT_FILE)
	-rm -rf $(OBJ_DIR)