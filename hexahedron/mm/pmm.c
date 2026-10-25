/**
 * @file hexahedron/mm/pmm.c
 * @brief PMM
 * 
 * This is a coalescing free-list allocator, which is like a buddy but easier to write + less versatile.
 * O(1) allocation for one page, O(n) for allocating contiguous pages
 * O(1) freeing one page, O(n) for pmm_freePages
 * Not lumping in section lookup with above, all operations are TECHNICALLY O(S) but would be the same with buddy. To be optimized never
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/mm/pmm.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <string.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:PMM", __VA_ARGS__)

/* PMM internal memory */
static size_t pmm_internal_memory = 0; // in bytes
static size_t pmm_memory_pages = 0; // # of pages

/* Memory tracking */
static size_t pmm_total_blocks = 0;
static size_t pmm_used_blocks = 0;

/* Sections */
pmm_section_t *pmm_zones[NZONES] = { 0 };

static char *pmm_memory_types[] = {
    [PHYS_MEMORY_AVAILABLE] = "AVAILABLE",
    [PHYS_MEMORY_RESERVED] = "RESERVED",
    [PHYS_MEMORY_ACPI_NVS] = "ACPI_NVS",
    [PHYS_MEMORY_ACPI_RECLAIMABLE] = "ACPI_RCLM",
    [PHYS_MEMORY_BADRAM] = "BAD_RAM",
    [PHYS_MEMORY_KERNEL] = "KERNEL",
    [PHYS_MEMORY_MODULE] = "MODULE"
};


/**
 * @brief Init function to insert section into zone
 */
static pmm_section_t *pmm_insertSection(int zone, pmm_region_t *region) {
    // Calculate the size needed to hold everything
    size_t header_size = PAGE_ALIGN_UP(sizeof(pmm_section_t));
    if (header_size >= region->end - region->start) return NULL;
    pmm_internal_memory += header_size;

    if (region->type != PHYS_MEMORY_AVAILABLE) return NULL;
    pmm_section_t *section = (pmm_section_t*)arch_mmu_remap_physical(region->start, header_size, REMAP_PERMANENT);

    // Setup the section
    MUTEX_INIT(&section->lock);
    section->start = region->start + header_size;
    section->npages = (region->end - section->start) / PAGE_SIZE;
    section->nfree = section->npages;
    section->next = NULL;
    section->pages = NULL;
    section->first_run = NULL; // page array must be allocated and setup before a run can be created

    pmm_total_blocks += section->npages;

    if (pmm_zones[zone] == NULL) {
        pmm_zones[zone] = section;
        return section;
    }

    pmm_section_t *tgt = pmm_zones[zone];
    while (tgt->next) tgt = tgt->next;
    tgt->next = section;
    return section;
}

/**
 * @brief Initialize the physical memory manager
 */
void pmm_init(pmm_region_t *region) {
    pmm_region_t *r = region;

    // Build the sections first
    pmm_section_t *biggest = NULL;
    while (r) {
        LOG(DEBUG, "PMM entry %016llX - %016llX (%s)\n", r->start, r->end, pmm_memory_types[r->type]);

        r->start = PAGE_ALIGN_UP(r->start);
        r->end = PAGE_ALIGN_DOWN(r->end); // Shouldn't cause any problems...

        if (r->start >= r->end) {
            r = r->next;
            continue;
        }

        if (r->type == PHYS_MEMORY_AVAILABLE) {
            // Create sections for available regions
            pmm_section_t *s = pmm_insertSection(ZONE_DEFAULT, r);
            if (s && (!biggest || s->npages > biggest->npages)) biggest = s;
            if (s) pmm_memory_pages += s->npages;
        }

        r = r->next;
    }

    assert(biggest != NULL);

    //
    size_t page_array_bytes = 0;
    for (int zi = 0; zi < NZONES; zi++) {
        for (pmm_section_t *s = pmm_zones[zi]; s; s = s->next) {
            page_array_bytes += PAGE_ALIGN_UP(s->npages * sizeof(pmm_page_t));
        }
    }

    // TODO find another way to do this
    // The best method imo is using the memory map itself to allocate the necessary pages for the pfndb but for now
    // just take from biggest section
    assert(page_array_bytes < biggest->npages * PAGE_SIZE);

    size_t reserved_pages = page_array_bytes / PAGE_SIZE;
    uintptr_t bo = biggest->start;
    for (int zi = 0; zi < NZONES; zi++) {
        pmm_section_t *s = pmm_zones[zi];
        while (s) {
            size_t bytes = PAGE_ALIGN_UP(s->npages * sizeof(pmm_page_t));

            // Create page array
            s->pages = (pmm_page_t*)arch_mmu_remap_physical(bo, bytes, REMAP_PERMANENT);
            memset(s->pages, 0, bytes);

            // The largest section also contains the page arrays themselves
            size_t first_free = s == biggest ? reserved_pages : 0;
            s->nfree = s->npages - first_free;
            for (size_t idx = 0; idx < s->npages; idx++) {
                pmm_page_t *page = &s->pages[idx];
                page->sect = s;
                if (idx < first_free) page->refcount = 1;
                else page->flags = PAGE_FLAG_FREE;
            }

            if (s->nfree) {
                pmm_page_t *first = &s->pages[first_free];
                first->run.npages = s->nfree;
                first->run.run_prev = SIZE_MAX;
                first->run.run_next = SIZE_MAX;
                s->pages[s->npages - 1].run.npages = s->nfree;
                s->first_run = first;
            }

            // Go on
            bo += bytes;

            s = s->next;
        }
    }

    pmm_internal_memory += page_array_bytes;
    pmm_used_blocks += reserved_pages;

    LOG(INFO, "PMM using %d pages internally out of %d total pages\n", pmm_internal_memory / PAGE_SIZE, pmm_memory_pages);
}

/**
 * @brief PMM out of memory condition
 */
void pmm_oom(uintptr_t req) {
    kernel_panic_prepare(OUT_OF_MEMORY);

    dprintf(NOHEADER, "*** PMM detected OOM condition while allocating %d pages.\n\n", req);
    dprintf(NOHEADER, "A total of %d pages (%d kB) were reserved by the PMM\n", pmm_internal_memory / PAGE_SIZE, pmm_internal_memory / 1024);
    dprintf(NOHEADER, "The system had a total of %d kB of RAM available\n", pmm_memory_pages * PAGE_SIZE / 1024);
    dprintf(NOHEADER, "The PMM used %d blocks (%d kB) out of %d total (%d kB)\n\n", pmm_used_blocks, pmm_used_blocks * 4096 / 1024, pmm_total_blocks, pmm_total_blocks * 4096 / 1024);

    kernel_panic_finalize();
}


/**
 * @brief Allocate a PMM page
 * @param zone The zone to allocate from
 * @returns The page or 0x0 for no memory
 */
uintptr_t pmm_allocatePage(pmm_zone_t zone) {
    pmm_section_t *s = pmm_zones[zone];
    while (true) {
        while (s && __atomic_load_n(&s->nfree, __ATOMIC_RELAXED) == 0) {
            s = s->next;
        }

        if (!s) pmm_oom(1);

        // acquire lock, check nfree, if the race was lost retry
        mutex_acquire(&s->lock);
        if (s->nfree) break;
        mutex_release(&s->lock);
        s = pmm_zones[zone];
    }

    // we exited holding the section's lock
    assert(s->first_run != NULL);
    pmm_page_t *rf = s->first_run;

    // this can be the only page in the run or just the first
    rf->run.npages -= 1;
    if (rf->run.npages > 0) {
        // this is not the only page in the run, simply expand up
        // !!! TODO: coalesce nearby free regions together, since a run crossing 2 sections is not possible here
        pmm_page_t *np = (pmm_page_t*)((uintptr_t)rf + sizeof(pmm_page_t));
        np->run.npages = rf->run.npages;
        np->run.run_next = rf->run.run_next;
        np->run.run_prev = rf->run.run_prev;

        if (np->run.run_next != SIZE_MAX) {
            s->pages[np->run.run_next].run.run_prev += 1; // we've shifted the head forwards
        }

        // the last page needs to be updated too
        size_t first_idx = rf - s->pages;
        s->pages[first_idx + rf->run.npages].run.npages = rf->run.npages;

        s->first_run = np;
    } else {
        // this is the only page in the run
        if (rf->run.run_next == SIZE_MAX) {
            s->first_run = NULL;
        } else {
            // swap out the runs
            pmm_page_t *nr = &s->pages[rf->run.run_next];
            nr->run.run_prev = SIZE_MAX;
            s->first_run = nr;
        }
    }

    // done
    assert(rf->flags & PAGE_FLAG_FREE);
    rf->flags &= ~(PAGE_FLAG_FREE);
    rf->refcount = 1;
    __atomic_sub_fetch(&s->nfree, 1, __ATOMIC_RELAXED);
    mutex_release(&s->lock);

    __atomic_add_fetch(&pmm_used_blocks, 1, __ATOMIC_RELAXED);
    
    size_t pgidx = ((uintptr_t)rf - (uintptr_t)s->pages) / sizeof(pmm_page_t);
    return (uintptr_t)(s->start + pgidx * PAGE_SIZE);
}

/**
 * @brief Allocate multiple PMM pages
 * @param npages The amount of pages to allocate
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePages(size_t npages, pmm_zone_t zone) {
    if (npages == 0) return 0x0;
    if (npages == 1) return pmm_allocatePage(zone);

    for (pmm_section_t *s = pmm_zones[zone]; s; s = s->next) {
        if (__atomic_load_n(&s->nfree, __ATOMIC_RELAXED) < npages) continue;

        mutex_acquire(&s->lock);
        if (s->nfree < npages) {
            mutex_release(&s->lock);
            continue;
        }

        // we still need a large enough run to allocate from
        pmm_page_t *run = s->first_run;
        while (run && run->run.npages < npages) {
            size_t next = run->run.run_next;
            run = next == SIZE_MAX ? NULL : &s->pages[next];
        }

        if (!run) {
            mutex_release(&s->lock);
            continue;
        }

        size_t start = run - s->pages;
        size_t remaining = run->run.npages - npages;
        size_t prev = run->run.run_prev;
        size_t next = run->run.run_next;

        if (remaining) {
            // turn the new hanging tail into a free run
            size_t new_start = start + npages;
            pmm_page_t *new_run = &s->pages[new_start];
            new_run->run.npages = remaining;
            new_run->run.run_prev = prev;
            new_run->run.run_next = next;
            s->pages[new_start + remaining - 1].run.npages = remaining;

            if (prev == SIZE_MAX) s->first_run = new_run;
            else s->pages[prev].run.run_next = new_start;
            if (next != SIZE_MAX) s->pages[next].run.run_prev = new_start;
        } else {
            // eat the entire run
            if (prev == SIZE_MAX) s->first_run = next == SIZE_MAX ? NULL : &s->pages[next];
            else s->pages[prev].run.run_next = next;
            if (next != SIZE_MAX) s->pages[next].run.run_prev = prev;
        }

        // unmark pages
        for (size_t i = start; i < start + npages; i++) {
            pmm_page_t *pg = &s->pages[i];
            assert(pg->flags & PAGE_FLAG_FREE);
            assert(pg->refcount == 0);
            pg->flags &= ~PAGE_FLAG_FREE;
            pg->refcount = 1;
        }

        __atomic_sub_fetch(&s->nfree, npages, __ATOMIC_RELAXED);
        mutex_release(&s->lock);

        __atomic_add_fetch(&pmm_used_blocks, npages, __ATOMIC_RELAXED);
        return s->start + start * PAGE_SIZE;
    }

    pmm_oom(npages);
    return 0;
}

/**
 * @brief Release a reference to a PMM page
 * @param page The physical address of the page to release
 */
void pmm_release(uintptr_t page) {
    assert((page & (PAGE_SIZE - 1)) == 0);
    pmm_releasePage(pmm_page(page));
}

/**
 * @brief Free a PMM page
 * @param page The page to free
 */
void pmm_freePage(uintptr_t page) __attribute__((alias("pmm_release")));

/**
 * @brief Release a reference to a PMM page
 * @param pg The page metadata to release
 */
void pmm_releasePage(pmm_page_t *pg) {
    pmm_section_t *s = pg->sect;
    size_t idx = pg - s->pages;
    assert(idx < s->npages);
    mutex_acquire(&s->lock);

    assert(pg->refcount > 0);
    assert(!(pg->flags & PAGE_FLAG_FREE));

    if (__atomic_sub_fetch(&pg->refcount, 1, __ATOMIC_SEQ_CST) != 0) {
        mutex_release(&s->lock);
        return;
    }

    // this page is fully free now
    pg->inode = NULL;
    pg->ent = NULL;
    pg->offset = 0;
    pg->flags = PAGE_FLAG_FREE;

    // check the neighbors to see how to coalesce
    bool left_free = idx > 0 && (s->pages[idx - 1].flags & PAGE_FLAG_FREE);
    bool right_free = idx + 1 < s->npages && (s->pages[idx + 1].flags & PAGE_FLAG_FREE);

    if (left_free && right_free) {
        // case 1: both neighbors are free, coalesce the runs together maybe 
        size_t left_len = s->pages[idx - 1].run.npages;
        size_t left_idx = idx - left_len;
        size_t right_idx = idx + 1;
        pmm_page_t *right = &s->pages[right_idx];
        size_t right_len = right->run.npages;

        // remove right run and extend it over
        if (right->run.run_prev == SIZE_MAX) {
            s->first_run = right->run.run_next == SIZE_MAX ? NULL : &s->pages[right->run.run_next];
        } else {
            s->pages[right->run.run_prev].run.run_next = right->run.run_next;
        }
        if (right->run.run_next != SIZE_MAX) {
            s->pages[right->run.run_next].run.run_prev = right->run.run_prev;
        }

        size_t length = left_len + 1 + right_len;
        s->pages[left_idx].run.npages = length;
        s->pages[right_idx + right_len - 1].run.npages = length;
    } else if (left_free) {
        // case 2: the left one is free, coalesce back into it
        size_t left_len = s->pages[idx - 1].run.npages;
        size_t length = left_len + 1;
        s->pages[idx - left_len].run.npages = length;
        pg->run.npages = length;
    } else if (right_free) {
        // case 3: the right one is free, coalesce into it
        pmm_page_t *right = &s->pages[idx + 1];
        size_t length = right->run.npages + 1;
        pg->run.npages = length;
        pg->run.run_prev = right->run.run_prev;
        pg->run.run_next = right->run.run_next;
        if (pg->run.run_prev == SIZE_MAX) s->first_run = pg;
        else s->pages[pg->run.run_prev].run.run_next = idx;
        if (pg->run.run_next != SIZE_MAX) s->pages[pg->run.run_next].run.run_prev = idx;
        s->pages[idx + length - 1].run.npages = length;
    } else {
        // build a one page run
        pg->run.npages = 1;
        pg->run.run_prev = SIZE_MAX;
        pg->run.run_next = s->first_run ? (size_t)(s->first_run - s->pages) : SIZE_MAX;
        if (s->first_run) s->first_run->run.run_prev = idx;
        s->first_run = pg;
    }

    __atomic_add_fetch(&s->nfree, 1, __ATOMIC_RELAXED);
    mutex_release(&s->lock);

    __atomic_sub_fetch(&pmm_used_blocks, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Hold a page
 * @param page The page to hold
 * 
 * Increments the page refcount to prevent it from being freed.
 */
void pmm_retainPage(pmm_page_t *page) {
    pmm_section_t *s = page->sect;
    mutex_acquire(&s->lock); // !! do we actually need to hold the lock here?
    __atomic_add_fetch(&page->refcount, 1, __ATOMIC_SEQ_CST);
    mutex_release(&s->lock);
}

/**
 * @brief Hold a page
 * @param page The page to hold
 * 
 * Increments the page refcount to prevent it from being freed.
 */
void pmm_retain(uintptr_t page) {
    // Figure out the zone
    // TODO: this will actually be hard
    int zone = ZONE_DEFAULT;
    pmm_section_t *s = pmm_zones[zone];
    
    while (s && !(s->start + (s->npages * PAGE_SIZE) > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to retain %p but no section contains this block.", page);
    }

    size_t off = (page - s->start) / PAGE_SIZE;
    return pmm_retainPage(&s->pages[off]);
}

/**
 * @brief Get page
 */
pmm_page_t *pmm_page(uintptr_t page) {
    // Figure out the zone
    // TODO: this will actually be hard
    int zone = ZONE_DEFAULT;
    pmm_section_t *s = pmm_zones[zone];
    
    while (s && !(s->start + (s->npages * PAGE_SIZE) > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to get %p but no section contains this block.", page);
    }

    return &s->pages[((page-s->start) / PAGE_SIZE)];
}

/**
 * @brief Free PMM pages
 * @param page_base The page base
 * @param npages Amount of pages
 */
void pmm_freePages(uintptr_t page_base, size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        pmm_release(page_base + i * PAGE_SIZE);
    }
}

/**
 * @brief Get address
 */
uintptr_t pmm_address(pmm_page_t *page) {
    assert(((uintptr_t)page - (uintptr_t)page->sect->pages) % sizeof(pmm_page_t) == 0);
    uintptr_t offset = ((uintptr_t)page - (uintptr_t)page->sect->pages) / sizeof(pmm_page_t);
    return page->sect->start + (offset * PAGE_SIZE); 
}

/**
 * @brief Gets the total amount of blocks
 */
uintptr_t pmm_getTotalBlocks() {
    return pmm_total_blocks;
}

/**
 * @brief Gets the used amount of blocks
 */
uintptr_t pmm_getUsedBlocks() {
    return pmm_used_blocks;
}

/**
 * @brief Gets the free amount of blocks
 */
uintptr_t pmm_getFreeBlocks() {
    return pmm_total_blocks - pmm_used_blocks;
}
