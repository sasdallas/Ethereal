/**
 * @file hexahedron/mm/pmm.c
 * @brief Physical memory manager subsystem
 * 
 * Includes a POSSIBLE (but not actually implemented) zone subsystem
 * 
 * This PMM is a modified bitmap PMM with support for sectioning (resulting in faster allocations).
 * The beginning of sections are reserved for their bitmap and header data.
 * 
 * Note that this design is fundamentally flawed, requiring a pages array for page_t objects (meaning more allocations).
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/pmm.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:PMM", __VA_ARGS__)

/* Sections */
pmm_section_t *zones[NZONES] = { 0 };

/* PMM internal memory */
static size_t pmm_internal_memory = 0;
static uintptr_t pmm_memory_size = 0;

static sleep_queue_t _mq = {
    .lock = { 0 },
    .queue = { 0 },
};

mutex_t pmm_mutex = {
    .lock = -1,
    .name = "physical memory manager mutex",
    .queue = &_mq,
};

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
 * @brief PMM out of memory condition
 */
void pmm_oom(uintptr_t req) {
    kernel_panic_prepare(OUT_OF_MEMORY);

    dprintf(NOHEADER, "*** PMM detected OOM condition.\n\n");
    dprintf(NOHEADER, "A total of %d pages (%d kB) were reserved by the PMM\n", pmm_internal_memory / PAGE_SIZE, pmm_internal_memory / 1000);
    dprintf(NOHEADER, "The system had a total of %d kB of RAM available\n", pmm_memory_size / 1000);

    kernel_panic_finalize();
}


/**
 * @brief Init function to insert section into zone
 */
static pmm_section_t *pmm_insertSection(int zone, pmm_region_t *region) {
    // Calculate the size needed to hold everything
    size_t size = 0;
    size += sizeof(pmm_section_t); // Section header
    size += sizeof(sleep_queue_t);
    size += sizeof(mutex_t);
    size += (region->end - region->start) / PAGE_SIZE / 8; // Bitmap size
    
    size = PAGE_ALIGN_UP(size);

    pmm_internal_memory += size;

    if (size > region->end) {
        LOG(ERR, "Too many bytes are required to represent region so it cannot be added.\n");
        return NULL;
    }

    // Map the section
    pmm_section_t *section = (pmm_section_t*)arch_mmu_map_physical(region->start, sizeof(pmm_section_t) + sizeof(pmm_section_t) + sizeof(sleep_queue_t), REMAP_PERMANENT);
    
    // Create the mutex and sleep queue
    sleep_queue_t *s = (sleep_queue_t*)((uintptr_t)section + sizeof(pmm_section_t));
    memset(s, 0, sizeof(sleep_queue_t));
    mutex_t *m = (mutex_t*)((uintptr_t)section + sizeof(pmm_section_t) + sizeof(sleep_queue_t));
    __atomic_store_n(&m->lock, -1, __ATOMIC_SEQ_CST);
    m->name = "physical memory manager mutex";
    m->queue = s;


    // Setup the section
    section->start = region->start + size;
    section->size = region->end - region->start - size;
    section->mutex = m;
    section->bmap = (uint8_t*)arch_mmu_map_physical((region->start  + sizeof(pmm_section_t) + sizeof(mutex_t) + sizeof(sleep_queue_t)), (region->end - region->start) / PAGE_SIZE / 8, REMAP_PERMANENT);
    section->nfree = section->size / PAGE_SIZE;
    section->next = NULL;
    section->pages = NULL;
    section->ffb = 0;
    memset(section->bmap, 0, (region->end - region->start) / PAGE_SIZE / 8);

    if (zones[zone] == NULL) {
        zones[zone] = section;
        return section;
    }

    pmm_section_t *tgt = zones[zone];
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
    uintptr_t memory_size = 0;
    while (r) {
        LOG(DEBUG, "PMM entry %016llX - %016llX (%s)\n", r->start, r->end, pmm_memory_types[r->type]);

        if (r->type == PHYS_MEMORY_AVAILABLE) {
            // Create sections for available regions
            pmm_section_t *s = pmm_insertSection(ZONE_DEFAULT, r);
            if (r->start > memory_size) memory_size = r->start;
            if (!biggest || (s && (biggest->size) < (r->end - r->start))) biggest = s;
        }

        r = r->next;
    }
    
    pmm_memory_size = memory_size;


    // Now we need to build the page arrays for each section
    uintptr_t bo = biggest->start;
    for (int i = 0; i < NZONES; i++) {
        pmm_section_t *s = zones[i];
        while (s) {
            size_t size = PAGE_ALIGN_UP(s->size / PAGE_SIZE * sizeof(pmm_page_t));
            pmm_internal_memory += size;

            // Create page array
            s->pages = (pmm_page_t*)arch_mmu_map_physical(bo, size, REMAP_PERMANENT);
            memset(s->pages, 0, size);

            // Mark the pages
            for (unsigned i = 0; i < s->size / PAGE_SIZE; i++) {
                s->pages[i].flags |= PAGE_FLAG_FREE;
            }

            // Adjust nfree count
            biggest->nfree -= size / PAGE_SIZE;

            // Adjust for bmap size
            size_t svd = (bo - biggest->start);
            bo += size;
            size += svd;

            // Rewrite bitmap
            memset(biggest->bmap, 0xFF, size / PAGE_SIZE / 8);
            for (unsigned i = 0; i < (size / PAGE_SIZE) % 8; i++) {
                biggest->bmap[size / PAGE_SIZE / 8] |= (1 << i);
            }

            // Recalcaulte FFB
            biggest->ffb = size / PAGE_SIZE / 8;

            s = s->next;    
        }
    }


    // For biggest, we actually ended up using some of its pages. Fix that up.
    for (unsigned i = 0 ; i < (biggest->size / PAGE_SIZE) - biggest->nfree; i++) {
        biggest->pages[i].flags &= ~(PAGE_FLAG_FREE);
    }

    // DEBUG

    for (int i = 0; i < 158+18; i++) {
        pmm_allocatePage(ZONE_DEFAULT);
    }

    pmm_section_t *s = zones[ZONE_DEFAULT];
    while (s) {
        LOG(INFO, "PMM section %p - %p, number of free pages %d (FFB: %d) with page array %p - %p and bitmap %p\n", s->start, s->start + s->size, s->nfree, s->ffb, s->pages, (uintptr_t)s->pages + PAGE_ALIGN_UP(s->size / PAGE_SIZE * sizeof(pmm_page_t)), s->bmap);
        s = s->next;
    }

    LOG(INFO, "PMM using %d pages internally\n", pmm_internal_memory / PAGE_SIZE);

}

/**
 * @brief Allocate a PMM page
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePage(pmm_zone_t zone) {
    assert(zone >= 0 && zone < NZONES);

    // Get a section with a page
    // TODO: Check this for stability.. what if we skip a region right before a page gets freed? Probably not that big of a deal
    pmm_section_t *s = zones[zone];
    while (s && s->nfree == 0) s = s->next;
    if (!s) pmm_oom(1);

    mutex_acquire(s->mutex);

    // Use the FFB
    if (s->bmap[s->ffb] == 0xFF) {
        LOG(ERR, "FFB was not calculated correctly by last allocator or has been corrupted.\n");

        // Recalculate FFB
        while (s->bmap[s->ffb] == 0xFF) s->ffb++;
    }
    
    // Locate the first free bit
    uint8_t *byte = &s->bmap[s->ffb];
    int i = 0;
    for (; i < 8; i++) {
        if ((*byte & (1 << i)) == 0) {
            break;
        }
    }

    // Get the block number
    uintptr_t blk = (s->ffb * 8) + i;

    // Set the bit
    *byte |= (1 << i);

    // Recalculate FFB
    while (s->bmap[s->ffb] == 0xFF) s->ffb++;
    s->pages[blk].flags &= ~(PAGE_FLAG_FREE);

    s->nfree--;

    mutex_release(s->mutex);

    return s->start + (blk * 4096);
}

/**
 * @brief Allocate multiple PMM pages
 * @param npages The amount of pages to allocate
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePages(size_t npages, pmm_zone_t zone) {
    if (npages == 1) return pmm_allocatePage(zone);

    STUB();
}

/**
 * @brief Free a PMM page
 * @param page The page to free
 */
void pmm_freePage(uintptr_t page) {
    // Figure out the zone
    // TODO: this will actually be hard
    int zone = ZONE_DEFAULT;

    pmm_section_t *s = zones[zone];
    
    while (s && !(s->start + s->size > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to free %p but no section contains this block.", page);
    }

    mutex_acquire(s->mutex);

    int off = ((page - s->start) / PAGE_SIZE);
    assert(!((s->pages[off].flags & PAGE_FLAG_FREE) == PAGE_FLAG_FREE) && "Bitmap/page array inconsistency.");

    // Clear bit
    s->bmap[off / 8] &= ~(1 << (off % 8));
    if ((unsigned)off / 8 < s->ffb) s->ffb = off / 8;

    s->pages[off].flags |= PAGE_FLAG_FREE;

    mutex_release(s->mutex);
}

/**
 * @brief Free PMM pages
 * @param page_base The page base
 * @param npages Amount of pages
 */
void pmm_freePages(uintptr_t page_base, size_t npages) {
    STUB();
}