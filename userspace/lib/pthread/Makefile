# Userspace Library Makefile
# NOTE: YOU CAN MODIFY THIS IF YOU WANT TO ADD ADDITIONAL TARGETS - SEE BOTTOM OF FILE

include ../make.config

# Working directory
WORKING_DIR = $(shell pwd)

# Get the actual directory (e.g. storage/ahci) 
ACTUAL_DIR = $(patsubst $(root_userspace_dir)%,%,$(WORKING_DIR))

# Output directory
OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/userspace/$(ACTUAL_DIR)
OUTPUT_DIR = $(DESTDIR)$(LIBDIR)/

# Output file name
OUTPUT_FILE = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/get_userspace_lib_info.py $(WORKING_DIR) NAME)

# Source files
C_SRCS = $(shell find . -name "*.c" -printf '%f ')
C_OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SRCS))

# freetype-specific
# TODO: better way
CFLAGS += -I$(DESTDIR)$(INCLUDE_DIRECTORY)/freetype2

PRINT_HEADER:
	@echo "-- Building library \"$(OUTPUT_FILE)\""

MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)
	-@mkdir -p $(OUTPUT_DIR)

$(OBJ_DIR)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

./$(OUTPUT_FILE): $(C_OBJS)
	$(AR) rcs $@ $(C_OBJS)
	
install: MAKE_OUTPUT ./$(OUTPUT_FILE)
	cp -r $(OUTPUT_FILE) $(OUTPUT_DIR)/$(OUTPUT_FILE)
	rm ./$(OUTPUT_FILE)

	-mkdir -pv $(OUTPUT_DIR)/pkgconfig
	cp pthread.pc $(OUTPUT_DIR)/pkgconfig/

clean:
	-rm -rf $(OBJ_DIR)

