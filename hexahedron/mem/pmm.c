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

// Frames bitmap 
uintptr_t    *frames;
uintptr_t    nframes = 0;

// Debugging information
uintptr_t    pmm_memorySize = 0;
uintptr_t    pmm_usedBlocks = 0;
uintptr_t    pmm_maxBlocks = 0;

// Spinlock
static spinlock_t frame_lock = { 0 };

// Log method
#define LOG(status, ...) dprintf_module(status, "MEM:PMM", __VA_ARGS__)

/**
 * @brief Initialize the physical memory system.
 * @param memsize Available physical memory in bytes.
 * @param frames_bitmap The bitmap of frames allocated to the system (mapped into memory)
 */
int pmm_init(uintptr_t memsize, uintptr_t *frames_bitmap) {
    if (!memsize || !frames_bitmap) kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "physmem");

    pmm_memorySize = memsize;
    pmm_maxBlocks = pmm_memorySize / PMM_BLOCK_SIZE;
    pmm_usedBlocks = pmm_maxBlocks; // By default, all is in use. You must mark valid memory manually

    frames = frames_bitmap;
    nframes = pmm_maxBlocks;

    // Set the bitmap to be all in use by default
    memset(frames, 0xF, nframes / 8);

    return 0;
}

// Set a frame/bit in the frames array
void pmm_setFrame(int frame) {
    frames[PMM_INDEX_BIT(frame)] |= ((unsigned)1 << PMM_OFFSET_BIT(frame));
}

// Clear a frame/bit in the frames array
void pmm_clearFrame(int frame) {
    frames[PMM_INDEX_BIT(frame)] &= ~((unsigned)1 << PMM_OFFSET_BIT(frame));
}

// Test whether a frame is set
int pmm_testFrame(int frame) {
    return frames[PMM_INDEX_BIT(frame)] & ((unsigned)1 << PMM_OFFSET_BIT(frame));
}

// Find the first free frame
int pmm_findFirstFrame() {
    for (uint32_t i = 0; i < PMM_INDEX_BIT(nframes); i++) {
        if (frames[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(frames[i] & ((unsigned)1 << bit))) {
                    return i * 4 * 8 + bit;
                }
            }
        }
    }

    // No free frame
    return -ENOMEM;
}

// Find first n frames
int pmm_findFirstFrames(size_t n) {
    if (n == 0) return 0x0;
    if (n == 1) return pmm_findFirstFrame();

    for (uint32_t i = 0; i < PMM_INDEX_BIT(nframes); i++) {
        if (frames[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(frames[i] & ((unsigned)1 << bit))) {
                    // Found it! But are there enough?

                    int starting_bit = i * 32 + bit;

                    uint32_t free_bits = 0;
                    for (uint32_t check = 0; check < n; check++) {
                        if (!pmm_testFrame(starting_bit + check)) free_bits++;
                        else break;
                    }

                    if (free_bits == n) return i * 4 * 8 + bit;
                }
            }
        }
    }

    // Not enough free frames
    return -ENOMEM;
}

/**
 * @brief Initialize a region as available memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_initializeRegion(uintptr_t base, uintptr_t size) {
    if (!size) return;

    spinlock_acquire(&frame_lock);

    // Calculate align and blocks
    int align = (PMM_ALIGN(base) / PMM_BLOCK_SIZE);
    int blocks = PMM_ALIGN(size) / PMM_BLOCK_SIZE;

    // Check
    if ((uintptr_t)align > nframes) {
        // Nothing we can do
        LOG(ERR, "Cannot initialize address range %p - %p\n", base, base+size);
        return;
    }

    if ((uintptr_t)(align + blocks) > nframes) {
        LOG(WARN, "Initializing address range %p - %p instead of %p - %p\n", base, base+((nframes-(uintptr_t)align)*PMM_BLOCK_SIZE), base, base+size);
        blocks = (nframes - (uintptr_t)align);
    }

    for (; blocks > 0; blocks--) {
        pmm_clearFrame(align++);
        pmm_usedBlocks--;
    }

    spinlock_release(&frame_lock);
}

/**
 * @brief Initialize a region as unavailable memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_deinitializeRegion(uintptr_t base, uintptr_t size) {
    if (!size) return;

    spinlock_acquire(&frame_lock);

    // Careful not to cause a div by zero on base.
    int align = (base == 0) ? 0 : (PMM_ALIGN(base) / PMM_BLOCK_SIZE);
    int blocks = PMM_ALIGN(size) / PMM_BLOCK_SIZE;

    // Check
    if ((uintptr_t)align > nframes) {
        // Nothing we can do
        LOG(ERR, "Cannot deinitialize address range %p - %p\n", base, base+size);
        return;
    }

    if ((uintptr_t)(align + blocks) > nframes) {
        LOG(WARN, "Deinitializing address range %p - %p instead of %p - %p\n", base, (nframes-(uintptr_t)align), base, base+size);
        blocks = (nframes - (uintptr_t)align);
    }

    for (; blocks > 0; blocks--) {
        if (pmm_testFrame(align)) { align++; continue; } // Block already marked
        pmm_setFrame(align++);
        pmm_usedBlocks++;
    }

    spinlock_release(&frame_lock);
}

/**
 * @brief Allocate a block
 * @returns A pointer to the block. If we run out of memory it will critically fault
 */
uintptr_t pmm_allocateBlock() {
    if ((pmm_maxBlocks - pmm_usedBlocks) <= 0) {
        goto _oom;
    }

    spinlock_acquire(&frame_lock);

    int frame = pmm_findFirstFrame();
    if (frame == -ENOMEM) goto _oom;
    pmm_setFrame(frame);
    pmm_usedBlocks++;

    spinlock_release(&frame_lock);    
    return (uintptr_t)(frame * PMM_BLOCK_SIZE);

_oom:
    spinlock_release(&frame_lock);
    kernel_panic(OUT_OF_MEMORY, "physmem");
    __builtin_unreachable();
}

/**
 * @brief Frees a block
 * @param block The address of the block
 */
void pmm_freeBlock(uintptr_t block) {
    if (block % PMM_BLOCK_SIZE != 0) return;

    spinlock_acquire(&frame_lock);

    int frame = block / PMM_BLOCK_SIZE;
    pmm_clearFrame(frame);
    pmm_usedBlocks--;

    spinlock_release(&frame_lock);
}

/**
 * @brief Allocate @c blocks amount of blocks
 * @param blocks The amount of blocks to allocate. Must be aligned to PMM_BLOCK_SIZE - do not pass bytes!
 */
uintptr_t pmm_allocateBlocks(size_t blocks) {
    if (!blocks) kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "physmem");
    if ((pmm_maxBlocks - pmm_usedBlocks) <= blocks) kernel_panic(OUT_OF_MEMORY, "physmem");
    
    spinlock_acquire(&frame_lock);
    int frame = pmm_findFirstFrames(blocks);
    if (frame == -ENOMEM) {
        kernel_panic(OUT_OF_MEMORY, "physmem");
    }

    for (uint32_t i = 0; i < blocks; i++) {
        pmm_setFrame(frame + i);
    }

    pmm_usedBlocks += blocks;
    spinlock_release(&frame_lock);
    return (uintptr_t)(frame * PMM_BLOCK_SIZE);
}

/**
 * @brief Frees @c blocks amount of blocks
 * @param base Pointer returned by @c pmm_allocateBlocks
 * @param blocks The amount of blocks to free. Must be aligned to PMM_BLOCK_SIZE
 */
void pmm_freeBlocks(uintptr_t base, size_t blocks) {
    if (!blocks) return;
    
    spinlock_acquire(&frame_lock);

    int frame = base / PMM_BLOCK_SIZE;
    for (uint32_t i = 0; i < blocks; i++) pmm_clearFrame(frame + i);
    pmm_usedBlocks -= blocks;

    spinlock_release(&frame_lock);
}

/**
 * @brief Gets the physical memory size
 */
uintptr_t pmm_getPhysicalMemorySize() {
    return pmm_memorySize;
}

/**
 * @brief Gets the maximum amount of blocks
 */
uintptr_t pmm_getMaximumBlocks() {
    return pmm_maxBlocks;
}

/**
 * @brief Gets the used amount of blocks
 */
uintptr_t pmm_getUsedBlocks() {
    return pmm_usedBlocks;
}

/**
 * @brief Gets the free amount of blocks
 */
uintptr_t pmm_getFreeBlocks() {
    return (pmm_maxBlocks - pmm_usedBlocks);
}