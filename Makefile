BUILD_DIR=build-output/
SYSROOT=$(shell pwd)/$(BUILD_DIR)/sysroot/
export SYSROOT

$(BUILD_DIR)/CMakeCache.txt:
	@mkdir -p $(BUILD_DIR)/
	cmake -S . -B $(BUILD_DIR)/ --toolchain $(shell pwd)/ethereal-cmake.txt -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DHEADERS_ONLY=FALSE

$(BUILD_DIR)/headers/CMakeCache.txt:
	@mkdir -p $(BUILD_DIR)/headers
	cmake -S . -B $(BUILD_DIR)/headers/ --toolchain $(shell pwd)/ethereal-cmake.txt -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DHEADERS_ONLY=TRUE

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target install

initrd: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target initrd

iso: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target iso

all: $(BUILD_DIR)/CMakeCache.txt build initrd iso

headers: $(BUILD_DIR)/headers/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target install

clean:
	-cmake --build $(BUILD_DIR) --target clean
	-cmake --build $(BUILD_DIR)/headers/ --target clean

clean-sysroot:
	-rm -rf $(SYSROOT)

.PHONY: all
