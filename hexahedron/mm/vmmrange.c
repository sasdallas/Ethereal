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

/* Structure for managing the pages from which ranges can be pulled */
typedef struct vmm_range_page {
    struct vmm_range_page *next;        // Next page in the list
    struct vmm_range_page *prev;        // Previous page in the list
    mutex_t mut;                        // Mutex to protect the range page
    uint64_t bitmap[4];                 // Up to 256 entries able to be represented
    uint64_t rem;                       // Remaining objects in this bitmap pool
    vmm_memory_range_t ranges[];        // List of ranges follows immediately after
} vmm_range_page_t;

/* Amount of entries that a single page can fit */
const int _vmm_entries_per_page = (PAGE_SIZE - sizeof(vmm_range_page_t)) / sizeof(vmm_memory_range_t);

/* Head entry */
static vmm_range_page_t *vmm_ranges_head = NULL;


/**
 * @brief Find a free spot in a VMM context
 * @param space The space to search
 * @param address Address hint. If NULL, ignored.
 * @param size Size required for the region
 * @returns The start of the region or NULL on failure
 */
uintptr_t vmm_findFree(vmm_space_t *space, uintptr_t address, size_t size) {
    if (!space->range) return address;
    if (address ==  0x0) address = space->start;

    // Space at the beginning?
    vmm_memory_range_t *r = space->range;
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
    if (space->end - address >= size) {
        return address;
    }

    return 0x0;
}

/**
 * @brief Insert a new range into a VMM context
 * @param space The space to insert into
 * @param range The range to insert into the VMM context
 */
void vmm_insertRange(vmm_space_t *space, vmm_memory_range_t *range) {
    range->end = PAGE_ALIGN_DOWN(range->end);
    range->start = PAGE_ALIGN_DOWN(range->start);
    assert(range->end > range->start);
    assert(RANGE_IN_RANGE(range->start, range->end, space->start, space->end));


    if (!space->range) {
        space->range = range;
        return;
    }

    if (range->end < space->range->start) {
        space->range->prev = range;
        range->next = space->range;
        space->range = range;
        return;
    }

    vmm_memory_range_t *r = space->range;
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

    if (!(r->end <= range->start)) {
        dprintf(ERR, "Error inserting range %p - %p, target range %p - %p does not work\n", range->start, range->end, r->start, r->end);
        assert(0);
    }
    r->next = range;
    range->prev = r;
}

/**
 * @brief Returns a new range page
 */
static vmm_range_page_t *vmm_createRangePage() {
    vmm_range_page_t *pg = (vmm_range_page_t *)arch_mmu_remap_physical(pmm_allocatePage(ZONE_DEFAULT), PAGE_SIZE, REMAP_PERMANENT);
    memset(pg, 0, sizeof(vmm_range_page_t));
    MUTEX_INIT(&pg->mut);
    pg->rem = _vmm_entries_per_page;
    return pg;
}


/**
 * @brief Get the range containing an allocation
 * @param space The space
 * @param start Allocation start
 * @param size Size of allocation
 */
vmm_memory_range_t *vmm_getRange(vmm_space_t *space, uintptr_t start, size_t size) {
    vmm_memory_range_t *r = space->range;
    while (r) {
        if (RANGE_IN_RANGE(start, start+size, r->start, r->end)) {
            return r;
        }

        r = r->next;
    }  
    
    return NULL;
}

/**
 * @brief Create a new VMM range (doesn't add it)
 * @param start The start of the range
 * @param end The end of the range
 */
vmm_memory_range_t *vmm_createRange(uintptr_t start, uintptr_t end, vmm_flags_t vmm_flags, mmu_flags_t mmu_flags) {
    if (!vmm_ranges_head) {
        // !!!: We have nothing to acquire a lock on.. assume this is before SMP init..
        // TODO add locks
        vmm_ranges_head = vmm_createRangePage();
    }

    vmm_range_page_t *p = vmm_ranges_head;
    while (p) {
        mutex_acquire(&p->mut);

        if (p->rem) {
            // Search the bitmap
            for (int i = 0; i < _vmm_entries_per_page; i++) {
                if (!(p->bitmap[i / (sizeof(uint64_t)*8)] & (1 << (i % (sizeof(uint64_t)*8))))) {
                    // We have a free entry here, mark it.
                    p->bitmap[i / (sizeof(uint64_t)*8)] |= (1 << (i % (sizeof(uint64_t)*8)));
                    p->rem -= 1;

                    mutex_release(&p->mut);

                    vmm_memory_range_t *r = &p->ranges[i];
                    r->start = start;
                    r->end = end;
                    r->vmm_flags = vmm_flags;
                    r->mmu_flags = mmu_flags;
                    r->next = r->prev = NULL; 
                    return r;
                }
            }
        }

        // If there's no next pointer, add a new range page
        if (p->next == NULL) {
            // Create a new page
            p->next = vmm_createRangePage();
            p->next->prev = p;
        }

        mutex_release(&p->mut);
        p = p->next;
    }

    assert(0 && "Unreachable");
    __builtin_unreachable();
}


/**
 * @brief Destroy a VMM memory range
 * @param range The range to destroy
 */
void vmm_destroyRange(vmm_memory_range_t *range) {
    if (range->next) range->next->prev = range->prev;
    if (range->prev) range->prev->next = range->next;

    // First, destroy the range if it was allocated
    // !!!: Will need to update later, a lot of type support will be needed
    for (uintptr_t i = range->start; i < range->end; i += PAGE_SIZE) {
        mmu_flags_t fl = arch_mmu_read_flags(NULL, i);

        if (range->vmm_flags & VM_FLAG_FILE) {
            // TODO
        } else {
            if (range->vmm_flags & VM_FLAG_ALLOC && fl & MMU_FLAG_PRESENT && fl & MMU_FLAG_RW) {
                uintptr_t pg = arch_mmu_physical(NULL, i);
                pmm_freePage(pg);
            }
        }

        arch_mmu_unmap(NULL, i);
    }

    // Now get the range page
    vmm_range_page_t *p = (vmm_range_page_t *)PAGE_ALIGN_DOWN((uintptr_t)range);

    // Index
    int idx = (int)(range - p->ranges);
    int bits_per_word = sizeof(uint64_t) * 8;
    int word = idx / bits_per_word;
    int bit = idx % bits_per_word;

    // Clear the allocation
    mutex_acquire(&p->mut);
    assert(p->bitmap[word] & (1ULL << bit));
    p->bitmap[word] &= ~(1ULL << bit);
    p->rem += 1;
    mutex_release(&p->mut);

    // If the page is totally free, destroy it
    if (p->rem == _vmm_entries_per_page) {
        if (p->prev) p->prev->next = p->next;
        if (p->next) p->next->prev = p->prev;
        if (vmm_ranges_head == p) vmm_ranges_head = p->next;

        uintptr_t phys = arch_mmu_physical(NULL, (uintptr_t)p);
        assert(phys);
        pmm_freePage(phys);
        arch_mmu_unmap(NULL, (uintptr_t)p);
    }
}