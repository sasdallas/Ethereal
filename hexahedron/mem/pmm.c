/**
 * @file hexahedron/mem/pmm.c
 * @brief Physical memory manager 
 * 
 * This is the default Hexahedron PMM. It is a bitmap frame allocator.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

// Includes
#include <stdint.h>
#include <string.h>
#include <errno.h>

// Kernel includes
#include <kernel/mem/pmm.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/misc/spinlock.h>
#include <kernel/misc/util.h>

// Frames bitmap 
uintptr_t    *frames;
uintptr_t    nframes = 0;

// Debugging information
uintptr_t       pmm_memorySize = 0;
uintptr_t       pmm_usedBlocks = 0;
uintptr_t       pmm_maxBlocks = 0;
uintptr_t       pmm_reservedBlocks = 0;


// Log method
#define LOG(status, ...) dprintf_module(status, "MEM:PMM", __VA_ARGS__)


// Set a frame/bit in the frames array
void pmm_setFrame(int frame) {
    STUB();
}

// Clear a frame/bit in the frames array
void pmm_clearFrame(int frame) {
    STUB();
}

// Test whether a frame is set
int pmm_testFrame(int frame) {
    STUB();
}

// Find the first free frame
int pmm_findFirstFrame() {
    STUB();
}

// Find first n frames
int pmm_findFirstFrames(size_t n) {
    STUB();
}

/**
 * @brief Initialize a region as available memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_initializeRegion(uintptr_t base, uintptr_t size) {
    STUB();
}

/**
 * @brief Initialize a region as unavailable memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_deinitializeRegion(uintptr_t base, uintptr_t size) {
    STUB();
}

/**
 * @brief Allocate a block
 * @returns A pointer to the block. If we run out of memory it will critically fault
 */
uintptr_t pmm_allocateBlock() {
    STUB();
}

/**
 * @brief Frees a block
 * @param block The address of the block
 */
void pmm_freeBlock(uintptr_t block) {
    STUB();
}

/**
 * @brief Allocate @c blocks amount of blocks
 * @param blocks The amount of blocks to allocate. Must be aligned to PMM_BLOCK_SIZE - do not pass bytes!
 */
uintptr_t pmm_allocateBlocks(size_t blocks) {
    STUB();
}

/**
 * @brief Frees @c blocks amount of blocks
 * @param base Pointer returned by @c pmm_allocateBlocks
 * @param blocks The amount of blocks to free. Must be aligned to PMM_BLOCK_SIZE
 */
void pmm_freeBlocks(uintptr_t base, size_t blocks) {
    STUB();
}

/**
 * @brief Gets the physical memory size
 */
uintptr_t pmm_getPhysicalMemorySize() {
    STUB();
}

