# Userspace Library Makefile

include ../make.config

# Working directory
WORKING_DIR = $(shell pwd)

# Get the actual directory (e.g. storage/ahci) 
ACTUAL_DIR = $(patsubst $(root_userspace_dir)%,%,$(WORKING_DIR))

# Output directory
OBJ_DIR = $(OBJ_OUTPUT_DIRECTORY)/userspace/$(ACTUAL_DIR)
OUTPUT_DIR = $(DESTDIR)$(LIBDIR)/


# Source files
C_SRCS = $(shell find . -name "*.c" -type f)
C_OBJS = $(addprefix $(OBJ_DIR)/,$(C_SRCS:.c=.o))

# Always generate build rules before any target
.PHONY: MAKE_OUTPUT GENERATE_SHARED_MAYBE CREATE_LIBRARIES GENERATE_PC

generated_build_rules.mk: $(PROJECT_ROOT)/buildscripts/lib.py
	@echo -- Generating build rules
	-@rm -f generated_build_rules.mk
	
	$(PYTHON) $< --generate-build-rules $(WORKING_DIR)/lib.conf


include generated_build_rules.mk

# Ensure build rules are generated before all main targets
install: MAKE_OUTPUT CREATE_LIBRARIES GENERATE_SHARED_MAYBE GENERATE_PC


MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)
	-@mkdir -p $(OUTPUT_DIR)

$(OBJ_DIR)/%.o: %.c
	-@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(LIB_SHARED), 1)
GENERATE_SHARED_MAYBE:
	$(CC) -shared -o $(OUTPUT_DIR)$(LIB_PREFIX).so $(C_OBJS)
	$(PYTHON) $(PROJECT_ROOT)/buildscripts/lib.py --build_symlinks $(WORKING_DIR)/lib.conf
else
GENERATE_SHARED_MAYBE:
endif

GENERATE_PC:
	$(PYTHON) $(PROJECT_ROOT)/buildscripts/genpc.py $(WORKING_DIR)/lib.conf

CREATE_LIBRARIES: $(C_OBJS)
	$(AR) rcs $(OUTPUT_DIR)$(LIB_PREFIX).a $(C_OBJS)

clean:
	-rm -rf $(OBJ_DIR)
	-@rm generated_build_rules.mk
