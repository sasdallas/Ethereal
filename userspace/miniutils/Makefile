# Userspace Makefile
# NOTE: YOU CAN MODIFY THIS IF YOU WANT TO ADD ADDITIONAL TARGETS - SEE BOTTOM OF FILE

include ../make.config

# Working directory
WORKING_DIR = $(shell pwd)

# Get the actual directory (e.g. storage/ahci) 
ACTUAL_DIR = $(patsubst $(root_userspace_dir)%,%,$(WORKING_DIR))

# Output directory
OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/userspace/$(ACTUAL_DIR)

# Source files
C_SRCS = $(shell find . -name "*.c" -printf '%f ')
C_OUTPUTS = $(patsubst %.c, $(OBJ_DIR)/%, $(C_SRCS))


MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)
	-@mkdir -p $(INITRD)/bin/

$(OBJ_DIR)/%: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) $@.o $(LDFLAGS) -o $@
	cp -r $@ $(DESTDIR)$(EXEC_PREFIX)
	cp -r $@ $(INITRD)/bin

install: MAKE_OUTPUT $(C_OUTPUTS)

clean:
	-rm -rf $(OBJ_DIR)

