/**
 * @file hexahedron/mm/vmmclone.c
 * @brief VMM cloning logic
 * 
 * Implements copy-on-write
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/vmm.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <kernel/processor_data.h>

/**
 * @brief Clone a previous context into a new context
 * @param ctx The context to clone
 * @returns A new context
 */
vmm_context_t *vmm_clone(vmm_context_t *ctx) {
    vmm_context_t *new_ctx = vmm_createContext();
    
    assert(current_cpu->current_context == ctx);

    mutex_acquire(ctx->space->mut);

    vmm_memory_range_t *range = ctx->space->range;

    while (range) {
        vmm_memory_range_t *nrange = vmm_createRange(range->start, range->end, range->vmm_flags, range->mmu_flags);
        vmm_insertRange(new_ctx->space, nrange);

        // Copy the data
        for (uintptr_t i = nrange->start; i < nrange->end; i += PAGE_SIZE) {
            if ((arch_mmu_read_flags(NULL, i) & MMU_FLAG_PRESENT) == 0) {
                continue;
            }

            uintptr_t phys = arch_mmu_physical(NULL, i);

            if (nrange->vmm_flags & VM_FLAG_DEVICE) {
                // Device memory, such as the framebuffer, should never be freed or copied.
                arch_mmu_map(new_ctx->dir, i, phys, nrange->mmu_flags);
                continue;
            }

#ifndef DISABLE_COW
            // Retain the physical page
            if (nrange->vmm_flags & VM_FLAG_SHARED) {
                pmm_retain(phys);
                arch_mmu_map(new_ctx->dir, i, phys, nrange->mmu_flags); // shared memory
            } else {
                arch_mmu_map(NULL, i, phys, nrange->mmu_flags & ~MMU_FLAG_WRITE);
                pmm_retain(phys);
                arch_mmu_map(new_ctx->dir, i, phys, nrange->mmu_flags & ~MMU_FLAG_WRITE);
            }
#else
            if (nrange->vmm_flags & VM_FLAG_SHARED) {
                pmm_retain(phys);
                arch_mmu_map(new_ctx->dir, i, phys, nrange->mmu_flags);
            } else {
                uintptr_t new = pmm_allocatePage(ZONE_DEFAULT);
                uintptr_t m1 = arch_mmu_remap_physical(new, PAGE_SIZE, REMAP_TEMPORARY);
                memcpy((void*)m1, (void*)i, PAGE_SIZE);
                arch_mmu_unmap_physical(m1, PAGE_SIZE);
                arch_mmu_map(new_ctx->dir, i, new, nrange->mmu_flags);
            }
#endif

        }

        range = range->next;
    }

    mutex_release(ctx->space->mut);

    return new_ctx;
}