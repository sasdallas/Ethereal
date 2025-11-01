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
#include <kernel/misc/util.h>

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
    STUB();
}

/**
 * @brief Allocate a DMA region from the kernel
 * 
 * DMA regions are contiguous blocks that currently cannot be destroyed
 */
uintptr_t mem_allocateDMA(uintptr_t size) {
    STUB();
}

/**
 * @brief Unallocate a DMA region from the kernel
 * 
 * @param base The address returned by @c mem_allocateDMA
 * @param size The size of the base
 */
void mem_freeDMA(uintptr_t base, uintptr_t size) {
    STUB();
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
    STUB();
}

/**
 * @brief Unmap an MMIO region
 * @param virt The virtual address returned by @c mem_mapMMIO
 */
void mem_unmapMMIO(uintptr_t virt, uintptr_t size) {
    STUB();
}

/**
 * @brief Map a driver into memory
 * @param size The size of the driver in memory
 * @returns A pointer to the mapped space
 */
uintptr_t mem_mapDriver(size_t size) {
    STUB();
}

/**
 * @brief Unmap a driver from memory
 * @param base The base address of the driver in memory
 * @param size The size of the driver in memory
 */
void mem_unmapDriver(uintptr_t base, size_t size) {
    STUB();
}

/**
 * @brief Get amount of memory in use by DMA
 */
uintptr_t mem_getDMAUsage() {
    STUB();
}

/**
 * @brief Get amount of memory in use by MMIO
 */
uintptr_t mem_getMMIOUsage() {
    STUB();
}

/**
 * @brief Get amount of memory in use by drivers
 */
uintptr_t mem_getDriverUsage() {
    STUB();
}