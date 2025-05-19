include ./make.config

# Define musl directories
MUSL_DIRECTORIES = math

# Include
CFLAGS += -Iinclude -Iinclude/internal -D__MUSL__

# Get the architecture subfolder
include build/$(BUILD_ARCH).conf

# Object stuff
MUSL_OBJ = $(OBJ_OUTPUT_DIRECTORY)/musl
OUTPUT_DIRECTORIES_MUSL = $(addprefix $(MUSL_OBJ)/,$(MUSL_DIRECTORIES))

# musl sources. We will compile in this Makefile.
MUSL_SRCS := $(shell find  $(MUSL_DIRECTORIES) -maxdepth 1 -name '*.c' ) 
MUSL_OBJS := $(patsubst %.c, $(MUSL_OBJ)/%.o, $(MUSL_SRCS))

$(MUSL_OBJ)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(MUSL_OBJ)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(MUSL_OBJ)/libm.a: $(MUSL_OBJS)
	$(AR) rcu $@ $(MUSL_OBJS)

MAKE_DIRECTORIES:
	@-mkdir -pv $(MUSL_OBJ)
	@-mkdir -pv $(DESTDIR)$(LIB_DIRECTORY)
	@-mkdir -pv $(OUTPUT_DIRECTORIES_MUSL)

install: MAKE_DIRECTORIES $(MUSL_OBJ)/libm.a
	-cp -r $(MUSL_OBJ)/libm.a $(DESTDIR)$(LIB_DIRECTORY)/

install-headers:
	@-mkdir -pv $(DESTDIR)$(INCLUDE_DIRECTORY)
	@-cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDE_DIRECTORY)/

clean:
	-rm -rf $(MUSL_OBJ)