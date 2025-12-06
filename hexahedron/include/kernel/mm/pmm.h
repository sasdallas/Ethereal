/**
 * @file hexahedron/include/kernel/mm/pmm.h
 * @brief Physical memory manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_PMM_H
#define KERNEL_MM_PMM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <kernel/misc/mutex.h>

/**** DEFINITIONS ****/

#define PHYS_MEMORY_AVAILABLE           0       // Available
#define PHYS_MEMORY_RESERVED            1       // Reserved
#define PHYS_MEMORY_ACPI_NVS            2       // ACPI NVS 
#define PHYS_MEMORY_ACPI_RECLAIMABLE    3       // ACPI reclaimable
#define PHYS_MEMORY_BADRAM              4       // Bad RAM
#define PHYS_MEMORY_MODULE              5       // Contains a module for the kernel
#define PHYS_MEMORY_KERNEL              6       // Contains the kernel

/* Zones for PMM allocation */
#define NZONES 1
#define ZONE_DEFAULT                    0

/* Page flags */
#define PAGE_FLAG_FREE                  0x1


/**** TYPES ****/

typedef struct pmm_region {
    struct pmm_region *next;
    uintptr_t start;
    uintptr_t end;
    uint8_t type;
} pmm_region_t;

typedef int pmm_zone_t;

typedef struct pmm_page {
    int flags;              // Page flags  
} pmm_page_t;

typedef struct pmm_section {
    struct pmm_section *next; 
    mutex_t *mutex;             // Mutex for the section
    uintptr_t start;            // Start
    size_t size;                // Size of the region, in bytes
    uint8_t *bmap;              // Bitmap
    uint64_t ffb;               // First free byte index
    volatile uint64_t nfree;    // Number of free blocks
    pmm_page_t *pages;          // List of pages
} pmm_section_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the physical memory manager
 */
void pmm_init(pmm_region_t *region);

/**
 * @brief Allocate a PMM page
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePage(pmm_zone_t zone);

/**
 * @brief Allocate multiple PMM pages
 * @param npages The amount of pages to allocate
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePages(size_t npages, pmm_zone_t zone);

/**
 * @brief Free a PMM page
 * @param page The page to free
 */
void pmm_freePage(uintptr_t page);

/**
 * @brief Free PMM pages
 * @param page_base The page base
 * @param npages Amount of pages
 */
void pmm_freePages(uintptr_t page_base, size_t npages);

/**
 * @brief Gets the total amount of blocks
 */
uintptr_t pmm_getTotalBlocks();

/**
 * @brief Gets the used amount of blocks
 */
uintptr_t pmm_getUsedBlocks();

/**
 * @brief Gets the free amount of blocks
 */
uintptr_t pmm_getFreeBlocks();

#endif