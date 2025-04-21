/**
 * @file hexahedron/mem/regions.c
 * @brief Manages certain memory regions, such as DMA and userspace
 * 
 * 
 * Architecture implementations are required to define the following variables:
 * MEM_DMA_REGION
 * MEM_DMA_REGION_SIZE
 * MEM_MMIO_REGION
 * MEM_MMIO_REGION_SIZE
 * MEM_DRIVER_REGION
 * MEM_DRIVER_REGION_SIZE
 * MEM_USERSPACE_REGION
 * MEM_USERSPACE_REGION_SIZE
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/mem/regions.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/pmm.h>
#include <kernel/misc/pool.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <structs/hashmap.h>

/* DMA pool */
pool_t *dma_pool = NULL;

/* MMIO pool */
pool_t *mmio_pool = NULL;

/* Driver pool */
pool_t *driver_pool = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MEM:REGIONS", __VA_ARGS__)



/**
 * @brief Initialize the regions system
 * 
 * Call this after your memory system is fully initialized (heap ready)
 */
void mem_regionsInitialize() {
    dma_pool = pool_create("dma pool", PAGE_SIZE, MEM_DMA_REGION_SIZE, MEM_DMA_REGION, POOL_DEFAULT);
    mmio_pool = pool_create("mmio pool", PAGE_SIZE, MEM_MMIO_REGION_SIZE, MEM_MMIO_REGION, POOL_DEFAULT);
    driver_pool = pool_create("driver pool", PAGE_SIZE, MEM_DRIVER_REGION_SIZE, MEM_DRIVER_REGION, POOL_DEFAULT);

    LOG(INFO, "Initialized region system.\n");
    LOG(INFO, "DMA region = %p, MMIO region = %p, driver region = %p\n", MEM_DMA_REGION, MEM_MMIO_REGION, MEM_DRIVER_REGION);
}

/**
 * @brief Allocate a DMA region from the kernel
 * 
 * DMA regions are contiguous blocks that currently cannot be destroyed
 */
uintptr_t mem_allocateDMA(uintptr_t size) {
    if (!size) return 0x0;

    if (!dma_pool) {
        LOG(WARN, "Function 0x%x attempted to allocate %d bytes from DMA buffer but regions are not ready\n", __builtin_return_address(0), size);
        return 0x0;
    }
    
    // Align size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    uintptr_t virt = pool_allocateChunks(dma_pool, size / PAGE_SIZE);

    // Success?
    if (virt == 0x0) {
        // No
        kernel_panic_extended(OUT_OF_MEMORY, "dma", "*** Could not allocate %d bytes for DMA\n", size);
    }

    uintptr_t phys = pmm_allocateBlocks(size / PMM_BLOCK_SIZE);

    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        mem_mapAddress(NULL, phys+i, virt+i, MEM_PAGE_KERNEL | MEM_PAGE_NOT_CACHEABLE);
    }

    return virt;
}

/**
 * @brief Unallocate a DMA region from the kernel
 * 
 * @param base The address returned by @c mem_allocateDMA
 * @param size The size of the base
 */
void mem_freeDMA(uintptr_t base, uintptr_t size) {
    if (!base) return;

    // Free the memory
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    pool_freeChunks(dma_pool, base, size / PAGE_SIZE);
}


/**
 * @brief Create an MMIO region
 * @param phys The physical address of the MMIO space
 * @param size Size of the requested space (must be aligned)
 * @returns Address to new mapped MMIO region
 * 
 * @warning MMIO regions cannot be destroyed.
 */
uintptr_t mem_mapMMIO(uintptr_t phys, size_t size) {
    if (!size || !phys) return 0x0;
    if (!mmio_pool) {
        LOG(WARN, "Function 0x%x attempted to allocate %d bytes from MMIO buffer but regions are not ready\n", __builtin_return_address(0), size);
        return 0x0;
    }

    // Align size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    // Get chunks
    uintptr_t virt = pool_allocateChunks(mmio_pool, size / PAGE_SIZE);

    // Success?
    if (virt == 0x0) {
        // No
        kernel_panic_extended(OUT_OF_MEMORY, "mmio", "*** Could not allocate %d bytes for MMIO\n", size);
    }

    // Map
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        mem_mapAddress(NULL, phys+i, virt+i, MEM_PAGE_KERNEL | MEM_PAGE_NOT_CACHEABLE);
    }

    return virt;
}

/**
 * @brief Unmap an MMIO region
 * @param virt The virtual address returned by @c mem_mapMMIO
 */
void mem_unmapMMIO(uintptr_t virt, uintptr_t size) {
    if (!virt) return;

    // Free the memory
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    pool_freeChunks(mmio_pool, virt, size / PAGE_SIZE);
}

/**
 * @brief Map a driver into memory
 * @param size The size of the driver in memory
 * @returns A pointer to the mapped space
 */
uintptr_t mem_mapDriver(size_t size) {
    if (!size) return 0x0;

    if (!driver_pool) {
        LOG(WARN, "Function 0x%x attempted to allocate %d bytes from driver buffer but regions are not ready\n", __builtin_return_address(0), size);
        return 0x0;
    }
    
    // Align size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    uintptr_t virt = pool_allocateChunks(driver_pool, size / PAGE_SIZE);

    // Success?
    if (virt == 0x0) {
        // No
        kernel_panic_extended(OUT_OF_MEMORY, "driver", "*** Could not allocate %d bytes for driver\n", size);
    }


    mem_allocate(virt, size, MEM_ALLOC_CRITICAL, MEM_PAGE_KERNEL);
    return virt;
}

/**
 * @brief Unmap a driver from memory
 * @param base The base address of the driver in memory
 * @param size The size of the driver in memory
 */
void mem_unmapDriver(uintptr_t base, size_t size) {
    if (!base) return;

    // Free the memory
    // Align size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    pool_freeChunks(driver_pool, base, size / PAGE_SIZE);
}