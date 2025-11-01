/**
 * @file hexahedron/include/kernel/mm/vmm.h
 * @brief Hexahedron VMM interface
 * 
 * The VMM interface of Hexahedron is inspired by the Astral VMM
 * https://github.com/Mathewnd/Astral/
 * 
 * This includes: MMU + VMM API, usage of slabs, VMM contexts, and probably a few other things.
 * No direct code is taken from the Astral kernel.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_VMM_H
#define KERNEL_MM_VMM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/mm/arch_mmu.h>
#include <kernel/mm/pmm.h>
#include <kernel/processor_data.h>
#include <kernel/misc/mutex.h>

/**** DEFINITIONS ****/

#define VMM_DEFAULT         0x0         // Default flags
#define VMM_ALLOCATE        0x1         // Allocate and back the pages
#define VMM_MUST_BE_EXACT   0x2         // Address hint must be matched exactly

/**** TYPES ****/

typedef uint64_t vmm_flags_t;

typedef struct vmm_memory_range {
    // Linked list fields
    struct vmm_memory_range *next;
    struct vmm_memory_range *prev;

    uintptr_t start;                    // MMU start address
    uintptr_t end;                      // MMU end address
    vmm_flags_t vmm_flags;              // VMM flags
    mmu_flags_t mmu_flags;              // MMU flags
} vmm_memory_range_t;

typedef struct vmm_context {
    mutex_t *mut;                       // Mutex
    vmm_memory_range_t *range;          // Range beginning
    uintptr_t start;
    uintptr_t end;
    mmu_dir_t *dir;                     // Directory
} vmm_context_t;

/**** VARIABLES ****/

extern vmm_context_t *vmm_kernel_context;

/**** MACROS ****/

#define VMM_IS_KERNEL_CTX() current_cpu->current_context == vmm_kernel_context

/**** FUNCTIONS ****/

/**
 * @brief Initialize the VMM
 * @param region The list of PMM regions to use
 */
void vmm_init(pmm_region_t *region);

/**
 * @brief Map VMM memory
 * @param addr The address to use
 * @param size The size to map in
 * @param vm_flags VM flags for the mapping
 * @param prot MMU protection flags 
 * @returns The address mapped or NULL on failure.
 */
void *vmm_map(void *addr, size_t size, vmm_flags_t vm_flags, mmu_flags_t prot);

/**
 * @brief Unmap/free VMM memory
 * @param addr The address to unmap
 * @param size The size of the region to unmap
 */
void vmm_unmap(void *addr, size_t size);

/**
 * @brief Switch VMM contexts
 * @param ctx The context to switch to
 */
void vmm_switch(vmm_context_t *ctx);

/**
 * @brief Dump all allocations in a context
 * @param context The context to dump
 */
void vmm_dumpContext(vmm_context_t *ctx);

/**
 * @brief Find a free spot in a VMM context
 * @param context The context to search
 * @param address Address hint. If NULL, ignored.
 * @param size Size required for the region
 * @returns The start of the region or NULL on failure
 */
uintptr_t vmm_findFree(vmm_context_t *ctx, uintptr_t address, size_t size);

/**
 * @brief Insert a new range into a VMM context
 * @param context The context to insert into
 * @param range The range to insert into the VMM context
 */
void vmm_insertRange(vmm_context_t *ctx, vmm_memory_range_t *range);


/**
 * @brief Create a new VMM range (doesn't add it)
 * @param start The start of the range
 * @param end The end of the range
 */
vmm_memory_range_t *vmm_createRange(uintptr_t start, uintptr_t end, vmm_flags_t vmm_flags, mmu_flags_t mmu_flags);

#endif