/**
 * @file hexahedron/mm/vmmrange.c
 * @brief Contains functions for modifying VMM ranges
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

/* !!!: HACK */
static uintptr_t vmm_range_pool_vaddr = 0;
static size_t vmm_range_pool_remaining = 0;

/**
 * @brief Find a free spot in a VMM context
 * @param context The context to search
 * @param address Address hint. If NULL, ignored.
 * @param size Size required for the region
 * @returns The start of the region or NULL on failure
 */
uintptr_t vmm_findFree(vmm_context_t *ctx, uintptr_t address, size_t size) {
    if (!ctx->range) return address;
    if (address ==  0x0) address = ctx->start;

    // Space at the beginning?
    vmm_memory_range_t *r = ctx->range;
    if (r->start > address && (r->start - address) >= size) {
        return address;
    }

    // Look for holes between the regions
    while (r) {
        if (!r->next) break;

        if (r->end < address) { r = r->next; continue; }
        if (r->end > address) address = r->end;

        // Calculate hole size
        size_t hole_size = r->next->start - address;
        if (hole_size >= size) {
            // We have enough
            return address;
        }

        r = r->next;
    }

    // Space from the end?
    if (ctx->end - address >= size) {
        return address;
    }

    return 0x0;
}

/**
 * @brief Insert a new range into a VMM context
 * @param context The context to insert into
 * @param range The range to insert into the VMM context
 */
void vmm_insertRange(vmm_context_t *ctx, vmm_memory_range_t *range) {
    range->end = PAGE_ALIGN_DOWN(range->end);
    range->start = PAGE_ALIGN_DOWN(range->start);
    assert(range->end > range->start);
    assert(RANGE_IN_RANGE(range->start, range->end, ctx->start, ctx->end));


    if (!ctx->range) {
        ctx->range = range;
        return;
    }

    if (range->end < ctx->range->start) {
        ctx->range->prev = range;
        range->next = ctx->range;
        ctx->range = range;
        return;
    }

    vmm_memory_range_t *r = ctx->range;
    while (r) {
        if (!r->next) break;

        uintptr_t hstart = r->end;
        uintptr_t hend = r->next->start;
        if (RANGE_IN_RANGE(range->start, range->end, hstart, hend)) {
            r->next->prev = range;
            range->next = r->next;
            range->prev = r;
            r->next = range;
            return;
        }

        r = r->next;
    }

    assert(r->end <= range->start);
    r->next = range;
    range->prev = r;
}

/**
 * @brief Create a new VMM range (doesn't add it)
 * @param start The start of the range
 * @param end The end of the range
 */
vmm_memory_range_t *vmm_createRange(uintptr_t start, uintptr_t end, vmm_flags_t vmm_flags, mmu_flags_t mmu_flags) {
    assert(end > start);
    start = PAGE_ALIGN_DOWN(start);
    end = PAGE_ALIGN_DOWN(end);

    const size_t obj_size = sizeof(vmm_memory_range_t);

    if (vmm_range_pool_remaining < obj_size) {
        if (vmm_range_pool_vaddr) arch_mmu_unmap_physical(vmm_range_pool_vaddr, PAGE_SIZE);

        uintptr_t phys = pmm_allocatePage(ZONE_DEFAULT);
        if (!phys) return NULL;
        vmm_range_pool_vaddr = arch_mmu_map_physical(phys, PAGE_SIZE, REMAP_TEMPORARY);
        vmm_range_pool_remaining =  PAGE_SIZE;
    }

    vmm_memory_range_t *range = (vmm_memory_range_t *)vmm_range_pool_vaddr;
    vmm_range_pool_vaddr += obj_size;
    vmm_range_pool_remaining -= obj_size;

    range->start = start;
    range->end = end;
    range->vmm_flags = vmm_flags;
    range->mmu_flags = mmu_flags;
    range->next = NULL;
    range->prev = NULL;

    return range;
}
