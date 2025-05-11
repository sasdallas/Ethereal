# Userspace Makefile
# NOTE: YOU CAN MODIFY THIS IF YOU WANT TO ADD ADDITIONAL TARGETS - SEE BOTTOM OF FILE

include ../make.config

# Working directory
WORKING_DIR = $(shell pwd)

# Get the actual directory (e.g. storage/ahci) 
ACTUAL_DIR = $(patsubst $(root_userspace_dir)%,%,$(WORKING_DIR))

# Output directory
OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/userspace/$(ACTUAL_DIR)
$(info $(OBJ_DIR))

# Source files
C_SRCS = $(shell find . -name "*.c" -printf '%f ')
C_OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SRCS))

# Output file name
OUTPUT_FILE = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) NAME)
OUTPUT_DIR = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) INSTALL_DIR)

INITRD_INSTALL = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) INITRD)

# Additional libraries
LIBS = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) LIBS)

PRINT_HEADER:
	@echo "-- Building app \"$(OUTPUT_FILE)\""

MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)
	-@mkdir -p $(OUTPUT_DIR)

$(OBJ_DIR)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

./$(OUTPUT_FILE): $(C_OBJS)
	$(LD) -o $(OUTPUT_FILE) $(C_OBJS) $(LDFLAGS) $(LIBS)

ifeq ($(INITRD_INSTALL),YES)
install: PRINT_HEADER MAKE_OUTPUT ./$(OUTPUT_FILE) $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) ADDITIONAL) install_initrd install_actual
else
install: PRINT_HEADER MAKE_OUTPUT ./$(OUTPUT_FILE) $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) ADDITIONAL) install_actual 
endif
	
install_actual:
	cp -r $(OUTPUT_FILE) $(DESTDIR)$(OUTPUT_DIR)/
	rm ./$(OUTPUT_FILE)

# hack
install_initrd:
	@-mkdir -p $(INITRD)/bin/
	cp -r $(OUTPUT_FILE) $(INITRD)/bin/

clean:
	-rm ./$(OUTPUT_FILE)
	-rm -rf $(DESTDIR)$(OUTPUT_DIR)/$(OUTPUT_FILE)
	-rm -rf $(OBJ_DIR)
	
bash_symlink:
	@-mkdir -p $(INITRD)/bin/
	-ln -s $(INITRD)/bin/$(OUTPUT_FILE) $(INITRD)/bin/sh
	-ln -s $(DESTDIR)$(OUTPUT_DIR)/$(OUTPUT_FILE) $(DESTDIR)$(OUTPUT_DIR)/sh	