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

# Always generate build rules before any target
generated_build_rules.mk: $(PROJECT_ROOT)/buildscripts/app.py app.conf
	@echo -- Generating build rules
	-@rm -f generated_build_rules.mk

# I should not be allowed near makefiles
	@set -e; \
	app_name="$$($(PYTHON) $< --name $(WORKING_DIR)/app.conf)";\
	cflags_add="$$($(PYTHON) $< --cflags $(WORKING_DIR)/app.conf)";\
	libs="$$($(PYTHON) $< --libs $(WORKING_DIR)/app.conf)";\
	ldflags_add="$$($(PYTHON) $< --ldflags $(WORKING_DIR)/app.conf)";\
	install_dir="$$($(PYTHON) $< --install-dir $(WORKING_DIR)/app.conf)";\
	echo -e "APP_NAME := $${app_name}\nLIBS := $${libs}\nCFLAGS += $${cflags_add}\nLDFLAGS += $${ldflags_add}\nINSTALL_DIR := $${install_dir}\n" > generated_build_rules.mk


include generated_build_rules.mk

TARGETS = $(shell $(PYTHON) $(PROJECT_ROOT)/buildscripts/app.py --additional-targets $(WORKING_DIR)/app.conf)

$(OBJ_DIR)/%.o: %.c
	-@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

install: install_real $(TARGETS)

install_real: MAKE_OUTPUT $(C_OBJS)
	$(CC) -o $(DESTDIR)$(INSTALL_DIR)/$(APP_NAME) $(C_OBJS) $(LDFLAGS) $(LIBS)
	$(PYTHON) $(PROJECT_ROOT)/buildscripts/app.py --create-symlinks $(WORKING_DIR)/app.conf

MAKE_OUTPUT:
	-@mkdir -p $(OBJ_DIR)

clean:
	-@rm generated_build_rules.mk

