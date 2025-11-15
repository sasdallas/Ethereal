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

            uintptr_t new_pg = pmm_allocatePage(ZONE_DEFAULT);
            
            uintptr_t new_pg_virt = arch_mmu_remap_physical(new_pg, PAGE_SIZE, REMAP_TEMPORARY);
            memcpy((void*)new_pg_virt, (void*)i, PAGE_SIZE);
            arch_mmu_unmap_physical(new_pg_virt, PAGE_SIZE);

            // TODO: CoW
            arch_mmu_map(new_ctx->dir, i, new_pg, nrange->mmu_flags);
        }

        range = range->next;
    }

    mutex_release(ctx->space->mut);

    return new_ctx;
}