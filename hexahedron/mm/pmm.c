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

/* Memory variables */
static volatile unsigned int pmm_total_blocks = 0;
static volatile unsigned int pmm_used_blocks = 0;

void pmm_debug();


mutex_t pmm_mutex = {
    .lock = -1,
    .name = "physical memory manager mutex",
    .queue = { .lock = { 0 } },
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

    dprintf(NOHEADER, "*** PMM detected OOM condition while allocating %d pages.\n\n", req);
    dprintf(NOHEADER, "A total of %d pages (%d kB) were reserved by the PMM\n", pmm_internal_memory / PAGE_SIZE, pmm_internal_memory / 1000);
    dprintf(NOHEADER, "The system had a total of %d kB of RAM available\n", pmm_memory_size / 1024);
    dprintf(NOHEADER, "The PMM used %d blocks (%d kB) out of %d total (%d kB)\n\n", pmm_used_blocks, pmm_used_blocks * 4096 / 1000, pmm_total_blocks, pmm_total_blocks * 4096 / 1000);

    pmm_debug();
    dprintf(NOHEADER, "\n");
    alloc_stats();

    kernel_panic_finalize();
}


/**
 * @brief Init function to insert section into zone
 */
static pmm_section_t *pmm_insertSection(int zone, pmm_region_t *region) {
    // Calculate the size needed to hold everything
    size_t n_pages = (region->end - region->start) / PAGE_SIZE;
    size_t bmap_bytes = (n_pages + 7) / 8;
    size_t header_size = sizeof(pmm_section_t) + sizeof(mutex_t) + bmap_bytes;
    size_t size = PAGE_ALIGN_UP(header_size);

    pmm_internal_memory += size;

    if (size > (region->end - region->start)) {
        LOG(ERR, "Too many bytes are required to represent region so it cannot be added.\n");
        return NULL;
    }

    // Map the entire header region (section + mutex + bitmap) contiguously
    pmm_section_t *section = (pmm_section_t*)arch_mmu_remap_physical(region->start, size, REMAP_PERMANENT);
    
    // Create the mutex and sleep queue
    mutex_t *m = (mutex_t*)((uintptr_t)section + sizeof(pmm_section_t));
    __atomic_store_n(&m->lock, -1, __ATOMIC_SEQ_CST);
    m->name = "physical memory manager mutex";
    memset(&m->queue, 0, sizeof(sleep_queue_t));


    // Setup the section
    section->start = region->start + size;
    section->size = region->end - region->start - size;
    section->mutex = m;
    section->bmap = (uint8_t*)((uintptr_t)section + sizeof(pmm_section_t) + sizeof(mutex_t));
    section->nfree = section->size / PAGE_SIZE;
    section->next = NULL;
    section->pages = NULL;
    section->ffb = 0;
    memset(section->bmap, 0, bmap_bytes);

    if (zones[zone] == NULL) {
        zones[zone] = section;
        return section;
    }

    pmm_section_t *tgt = zones[zone];
    while (tgt->next) tgt = tgt->next;
    tgt->next = section;

    pmm_total_blocks += section->size / PAGE_SIZE;
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

        r->end = PAGE_ALIGN_DOWN(r->end); // Shouldn't cause any problems...

        if (r->type == PHYS_MEMORY_AVAILABLE) {
            // Create sections for available regions
            pmm_section_t *s = pmm_insertSection(ZONE_DEFAULT, r);
            memory_size += (r->end - r->start);
            if (!biggest || (s && (biggest->size) < (r->end - r->start))) biggest = s;
        }

        r = r->next;
    }
    
    pmm_memory_size = memory_size;


    // Now we need to build the page arrays for each section
    if (!biggest) {
        LOG(ERR, "No biggest section found; cannot build page arrays\n");
        return;
    }

    uintptr_t bo = biggest->start;
    for (int zi = 0; zi < NZONES; zi++) {
        pmm_section_t *s = zones[zi];
        while (s) {
            size_t pages_in_section = s->size / PAGE_SIZE;
            size_t page_array_bytes = PAGE_ALIGN_UP(pages_in_section * sizeof(pmm_page_t));
            pmm_internal_memory += page_array_bytes;

            // Create page array
            s->pages = (pmm_page_t*)arch_mmu_remap_physical(bo, page_array_bytes, REMAP_PERMANENT);
            memset(s->pages, 0, page_array_bytes);

            // Mark the pages in the page array as free
            for (unsigned idx = 0; idx < pages_in_section; idx++) {
                s->pages[idx].flags |= PAGE_FLAG_FREE;
            }

            // Adjust nfree count to account for pages consumed to hold the page array
            biggest->nfree -= page_array_bytes / PAGE_SIZE;

            // Advance the physical offset where we place subsequent page arrays
            bo += page_array_bytes;

            // Compute how many pages in 'biggest' are now used to hold page arrays
            size_t total_used_pages = (bo - biggest->start) / PAGE_SIZE;
            if (total_used_pages > biggest->size / PAGE_SIZE) {
                LOG(ERR, "Not enough space in biggest section to store page arrays\n");
                assert(0 && "Insufficient space in biggest section for page arrays");
            }

            // Rewrite bitmap in 'biggest' to mark all pages up to total_used_pages as used
            size_t full_bytes = total_used_pages / 8;
            size_t rem_bits = total_used_pages % 8;

            if (full_bytes)
                memset(biggest->bmap, 0xFF, full_bytes);
            if (rem_bits) {
                biggest->bmap[full_bytes] |= (uint8_t)((1u << rem_bits) - 1);
            }

            // Recalculate FFB (first free byte index in bitmap)
            biggest->ffb = full_bytes;

            s = s->next;    
        }
    }


    // For biggest, we actually ended up using some of its pages. Fix that up.
    for (unsigned i = 0 ; i < (biggest->size / PAGE_SIZE) - biggest->nfree; i++) {
        biggest->pages[i].flags &= ~(PAGE_FLAG_FREE);
    }

    // DEBUG
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
        s->ffb = 0;
        while (s->bmap[s->ffb] == 0xFF) {
            if (s->ffb * 8 >= s->size / PAGE_SIZE) {
                assert(0 && "Bitmap is full, but Nfree does not reflect this.");
            }
            s->ffb++;
        }
    }
    
    // Locate the first free bit
    uint8_t *byte = &s->bmap[s->ffb];
    int i = 0;
    for (; i < 8; i++) {
        if ((*byte & (1 << i)) == 0) {
            break;
        }

        if ((s->ffb * 8 + i) >= s->size / PAGE_SIZE) {
            LOG(ERR, "Region can fit %d chunks, we are chunk %d, nfree %d\n", s->size / PAGE_SIZE, s->ffb * 8 + i, s->nfree);
            assert(0);
        }
    }

    // Get the block number
    uintptr_t blk = (s->ffb * 8) + i;

    // Set the bit
    *byte |= (1 << i);

    // Recalculate FFB
    while (s->bmap[s->ffb] == 0xFF) s->ffb++;

    // Setup page
    s->pages[blk].flags &= ~(PAGE_FLAG_FREE);
    if (s->pages[blk].refcount != 0) {
        LOG(ERR, "Crash imminent - refcount for page %p: %d\n", s->start + (blk * 4096), s->pages[blk].refcount);
        assert(0);
    }
    s->pages[blk].refcount = 1;

    s->nfree--;

    mutex_release(s->mutex);

    __atomic_add_fetch(&pmm_used_blocks, 1, __ATOMIC_RELAXED);

    return s->start + (blk * 4096);
}

/**
 * @brief Try allocate multiple pages
 */
static uintptr_t __pmm_try_section(pmm_section_t *s, size_t npages) {
    mutex_acquire(s->mutex);

    // Check to see if there's enough contiguous pages

    // Use the FFB
    if (s->bmap[s->ffb] == 0xFF) {
        LOG(ERR, "FFB was not calculated correctly by last allocator or has been corrupted.\n");

        // Recalculate FFB
        s->ffb = 0;
        while (s->bmap[s->ffb] == 0xFF) {
            if (s->ffb * 8 >= s->size / PAGE_SIZE) {
                assert(0 && "Bitmap is full, but Nfree does not reflect this.");
            }
            s->ffb++;
        }
    }
    

    while (1) {
        // We should have the lowest possible in s->ffb
        uint8_t byte = s->ffb;
    
        size_t total_pages = s->size / PAGE_SIZE;
        size_t start_byte = (size_t)byte;
        size_t bcount = (total_pages + 7) / 8;

        if (start_byte >= bcount) {
            mutex_release(s->mutex);
            return 0;
        }

        for (size_t by = start_byte; by < bcount; ++by) {
            if (s->bmap[by] == 0xFF) continue;

            int bit_start = (by == start_byte) ? (s->ffb % 8) : 0;
            for (int bit = bit_start; bit < 8; ++bit) {
                size_t st = by * 8 + bit;
                
                if (st + npages > total_pages) {
                    mutex_release(s->mutex);
                    return 0;
                }

                
                bool ok = true;
                for (size_t k = 0; k < npages; ++k) {
                    size_t idx = st + k;
                    if (s->bmap[idx / 8] & (1 << (idx % 8))) {
                        ok = false;
                        break;
                    }
                }

                if (!ok) continue;

                
                for (size_t k = 0; k < npages; ++k) {
                    size_t idx = st + k;
                    s->bmap[idx / 8] |= (1 << (idx % 8));
                    s->pages[idx].flags &= ~PAGE_FLAG_FREE;
                }

                s->nfree -= npages;

                
                while ((size_t)s->ffb < bcount && s->bmap[s->ffb] == 0xFF) {
                    s->ffb++;
                }

                mutex_release(s->mutex);
                __atomic_add_fetch(&pmm_used_blocks, npages, __ATOMIC_RELAXED);
                return s->start + (st * PAGE_SIZE);
            }
            

            mutex_release(s->mutex);
            return 0;
        }
    }

    assert(0);
}

/**
 * @brief Allocate multiple PMM pages
 * @param npages The amount of pages to allocate
 * @param zone The zone to allocate from
 */
uintptr_t pmm_allocatePages(size_t npages, pmm_zone_t zone) {
    if (npages == 1) return pmm_allocatePage(zone);
    pmm_section_t *s = zones[zone];

    while (1) {
        while (s && s->nfree < npages) s = s->next;
        if (!s) pmm_oom(npages);

        // Try and see if there's enough contig in this section
        uintptr_t try = __pmm_try_section(s, npages);
        if (try != 0x0) {
            return try;
        }
        
        s = s->next;
    }

    assert(0); // Unreachable
}

/**
 * @brief Free a PMM page
 * @param page The page to free
 */
void pmm_freePage(uintptr_t page) {
    pmm_release(page);
}

/**
 * @brief Free PMM pages
 * @param page_base The page base
 * @param npages Amount of pages
 */
void pmm_freePages(uintptr_t page_base, size_t npages) {
    for (size_t i = 0; i < npages; i++) pmm_release(page_base + (i * 4096));
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
    pmm_section_t *s = zones[zone];
    
    while (s && !(s->start + s->size > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to retain %p but no section contains this block.", page);
    }

    int off = (page - s->start) / PAGE_SIZE;
    mutex_acquire(s->mutex);
    __atomic_add_fetch(&s->pages[off].refcount, 1, __ATOMIC_SEQ_CST);
    mutex_release(s->mutex);
}

/**
 * @brief Release a page
 * @param page The page to release
 * 
 * Decrements the page refcount
 */
void pmm_release(uintptr_t page) {
    // Figure out the zone
    // TODO: this will actually be hard
    int zone = ZONE_DEFAULT;
    pmm_section_t *s = zones[zone];
    
    while (s && !(s->start + s->size > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to release %p but no section contains this block.", page);
    }

    int off = (page - s->start) / PAGE_SIZE;
    mutex_acquire(s->mutex);

    if (s->pages[off].refcount == 0) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Double free on page %p.\n", page);
    }

    uintptr_t ref = __atomic_sub_fetch(&s->pages[off].refcount, 1, __ATOMIC_SEQ_CST);
    if (ref == 0) {
        // empty page, remove its references
        s->pages[off].flags |= PAGE_FLAG_FREE;
        s->bmap[off / 8] &= ~(1 << (off % 8));
        if ((unsigned)off / 8 < s->ffb) s->ffb = off / 8;
        s->nfree++;    
        __atomic_sub_fetch(&pmm_used_blocks, 1, __ATOMIC_RELAXED);
    }

    mutex_release(s->mutex);
}

/**
 * @brief Get page
 */
pmm_page_t *pmm_page(uintptr_t page) {
    // Figure out the zone
    // TODO: this will actually be hard
    int zone = ZONE_DEFAULT;
    pmm_section_t *s = zones[zone];
    
    while (s && !(s->start + s->size > page && page >= s->start)) {
        s = s->next;
    }

    if (!s) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pmm", "*** Tried to get %p but no section contains this block.", page);
    }

    return &s->pages[((page-s->start) / PAGE_SIZE)];
}

/**
 * @brief debug
 */
void pmm_debug() {

    // DEBUG
    pmm_section_t *s = zones[ZONE_DEFAULT];
    while (s) {
        LOG(INFO, "PMM section %p - %p, number of free pages %d (FFB: %d) with page array %p - %p and bitmap %p\n", s->start, s->start + s->size, s->nfree, s->ffb, s->pages, (uintptr_t)s->pages + PAGE_ALIGN_UP(s->size / PAGE_SIZE * sizeof(pmm_page_t)), s->bmap);
        s = s->next;
    }

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
