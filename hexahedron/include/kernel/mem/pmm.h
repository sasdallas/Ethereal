/**
 * @file hexahedron/include/kernel/mem/pmm.h
 * @brief Physical memory manager header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_PMM_H
#define KERNEL_MEM_PMM_H

/**** INCLUDES ****/
#include <stdint.h>

/**** MACROS ****/
#define PMM_INDEX_BIT(a) (a / (8 * 4))
#define PMM_OFFSET_BIT(a) (a % (8 * 4))

/**** DEFINITIONS ****/
#define PMM_BLOCK_SIZE  4096

/**** FUNCTIONS ****/

/**
 * @brief Initialize the physical memory system.
 * @param memsize Available physical memory in bytes.
 * @param frames_bitmap The bitmap of frames allocated to the system (mapped into memory)
 */
int pmm_init(uintptr_t memsize, uintptr_t *frames_bitmap);

/**
 * @brief Initialize a region as available memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_initializeRegion(uintptr_t base, uintptr_t size);

/**
 * @brief Initialize a region as unavailable memory
 * @param base The starting address of the region
 * @param size The size of the region
 */
void pmm_deinitializeRegion(uintptr_t base, uintptr_t size);

/**
 * @brief Allocate a block
 * @returns A pointer to the block. If we run out of memory it will critically fault
 */
uintptr_t pmm_allocateBlock();

/**
 * @brief Frees a block
 * @param block The address of the block
 */
void pmm_freeBlock(uintptr_t block);

/**
 * @brief Allocate @c blocks amount of blocks
 * @param blocks The amount of blocks to allocate. Must be aligned to PMM_BLOCK_SIZE - do not pass bytes!
 */
uintptr_t pmm_allocateBlocks(size_t blocks);

/**
 * @brief Frees @c blocks amount of blocks
 * @param base Pointer returned by @c pmm_allocateBlocks
 * @param blocks The amount of blocks to free. Must be aligned to PMM_BLOCK_SIZE
 */
void pmm_freeBlocks(uintptr_t base, size_t blocks);

/**
 * @brief Gets the physical memory size
 */
uintptr_t pmm_getPhysicalMemorySize();

/**
 * @brief Gets the maximum amount of blocks
 */
uintptr_t pmm_getMaximumBlocks();

/**
 * @brief Gets the used amount of blocks
 */
uintptr_t pmm_getUsedBlocks();

/**
 * @brief Gets the free amount of blocks
 */
uintptr_t pmm_getFreeBlocks();


#endif