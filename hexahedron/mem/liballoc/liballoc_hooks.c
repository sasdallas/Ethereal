/**
 * @file hexahedron/mem/liballoc/liballoc_hooks.c
 * @brief liballoc hooks
 * 
 * @warning Due to the nature of liballoc, sbrk cannot be used
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "liballoc.h"
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/misc/spinlock.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/hal.h>

/*** ALLOCATOR SUBSYSTEM FUNCTIONS ***/

#define ALLOC_NAME "liballoc"
#define ALLOC_VERSION_MAJOR 1
#define ALLOC_VERSION_MINOR 1

static allocator_info_t *allocator_information = NULL; 

/**
 * @brief Get information on the allocator.
 */
allocator_info_t *alloc_getInfo() {
    // Allocate if not present.
    // !!!: This probably isn't a good idea. What happens if there was a fault in the allocator?
    // !!!: (contd) Should panic handle that or should we?
    if (allocator_information == NULL) {
        allocator_information = kmalloc(sizeof(allocator_info_t));
        strncpy((char*)allocator_information->name, ALLOC_NAME, 127);
        allocator_information->name[strlen(allocator_information->name)] = 0;

        allocator_information->version_major = ALLOC_VERSION_MAJOR;
        allocator_information->version_minor = ALLOC_VERSION_MINOR;

        allocator_information->support_valloc = 0;
    }
    
    return allocator_information;
}

/**
 * @brief Fake valloc hook
 */
void *alloc_valloc(size_t n) {
    return NULL;
}


/*** HOOKS ***/

/* Memory heap */
extern uintptr_t mem_kernelHeap;

/* Pointer to the first free page */
uintptr_t liballoc_first_free_page = 0x0;

/* Lock */
spinlock_t liballoc_spinlock = { 0 };

/* Log */
#define LOG(status, ...) dprintf_module(status, "ALLOC:LIBALLOC:HOOKS", __VA_ARGS__)

/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide. 
 *
 * \return 0 if the lock was acquired successfully. Anything else is
 * failure.
 */
int liballoc_lock() {
    spinlock_acquire(&liballoc_spinlock);
    return 0;
}

/** This function unlocks what was previously locked by the liballoc_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
int liballoc_unlock() {
    spinlock_release(&liballoc_spinlock);
    return 0;
}

/** This is the hook into the local system which allocates pages. It
 * accepts an integer parameter which is the number of pages
 * required.  The page size was set up in the liballoc_init function.
 *
 * \return NULL if the pages were not allocated.
 * \return A pointer to the allocated memory.
 */
void* liballoc_alloc(size_t n) {
    // !!!: Liballoc plays with the memory system in ways it wasn't supposed to. It manually checks for free pages
    if (!liballoc_first_free_page) liballoc_first_free_page = mem_getKernelHeap();
    uintptr_t search = liballoc_first_free_page;

    // From here we need to find n pages
    // !!!: This can easily crash. Need to load test and debug this.
    size_t found_pages = 0;
    uintptr_t start = 0x0; // Start of the pages
    while (1) {
        page_t *pg = mem_getPage(NULL, search, MEM_DEFAULT); // MEM_DEFAULT so we don't waste memory on creating pages
        if (pg && pg->bits.present) {
            // There does exist a page here, but does it have a frame?
            if (pg->bits.address) {
                // Yes, reset
                found_pages = 0;
                start = 0x0;
                goto _next_page;
            } else {
                LOG(WARN, "Found a present page at %p with no frame allocated. Using\n", search);
            }
        }
    
        // Found one
        found_pages++;
        if (!start) start = search;

        // All done?
        if (found_pages >= n) break;

    _next_page:
        search += PAGE_SIZE;
    }

    // Found the pages! Allocate them
    for (uintptr_t i = 0; i < n * PAGE_SIZE; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, start + i, MEM_CREATE);
        if (pg) mem_allocatePage(pg, MEM_PAGE_KERNEL); // !!!: Usermode allocations?
    }

    liballoc_first_free_page = start + (n * PAGE_SIZE);
    mem_kernelHeap = start + ((n+1) * PAGE_SIZE);

    return (void*)start;
}

/** This frees previously allocated memory. The void* parameter passed
 * to the function is the exact same value returned from a previous
 * liballoc_alloc call.
 *
 * The integer value is the number of pages to free.
 *
 * \return 0 if the memory was successfully freed.
 */
int liballoc_free(void* ptr, size_t n) {
    // Free the pages
    for (uintptr_t i = 0; i < n * PAGE_SIZE; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, (uintptr_t)ptr + i, MEM_DEFAULT);
        if (pg) mem_freePage(pg);
    }

    liballoc_first_free_page = (uintptr_t)ptr;
    return 0;
}