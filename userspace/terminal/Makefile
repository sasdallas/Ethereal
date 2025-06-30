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
	$(CC) -o $(OUTPUT_FILE) $(C_OBJS) $(LIBS) $(LDFLAGS)

install: PRINT_HEADER MAKE_OUTPUT ./$(OUTPUT_FILE) $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_app_info.py $(WORKING_DIR) ADDITIONAL) install_actual 

	
install_actual:
	cp -r $(OUTPUT_FILE) $(DESTDIR)$(OUTPUT_DIR)/
	rm ./$(OUTPUT_FILE)

clean:
	-rm ./$(OUTPUT_FILE)
	-rm -rf $(DESTDIR)$(OUTPUT_DIR)/$(OUTPUT_FILE)
	-rm -rf $(OBJ_DIR)