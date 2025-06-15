/**
 * @file hexahedron/mem/vas.c
 * @brief Virtual address space system
 * 
 * The virtual address system is Hexahedron's virtual address space manager
 * It is an improvement over the generic pool system which is used for allocating chunks.
 * The system is a linked list of memory objects that can be expanded
 * 
 * Hexahedron's process system will create VASes for each process. 
 * 
 * @todo CoW and proper destruction
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/mem/vas.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <string.h>

/* Helper macro */
#define ALLOC(n) (n->alloc)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MEM:VAS", __VA_ARGS__)


/**
 * @brief Create a new virtual address space in memory
 * @param name The name of the VAS (optional)
 * @param address The base address of the VAS
 * @param size The initial size of the VAS
 * @param flags VAS creation flags
 */
vas_t *vas_create(char *name, uintptr_t address, size_t size, int flags) {
    vas_t *vas = kmalloc(sizeof(vas_t));
    memset(vas, 0, sizeof(vas_t));

    vas->name = name;
    vas->base = address;
    vas->size = size;
    vas->flags = flags;
    vas->dir = mem_getCurrentDirectory();
    vas->lock = spinlock_create("vas lock");

    return vas;
}

/**
 * @brief Reserve a region in the VAS
 * @param vas The VAS to reserve in
 * @param address The region to reserve
 * @param size The size of the region to reserve
 * @param type The type of allocation
 * @returns The created allocation or NULL on failure
 * 
 * @warning Use this ONLY if you plan on the kernel allocating the pages. This will just reserve a region for you.
 * 
 * @note    This is a bit slow due to sanity checks and sorting, I'm sure it could be improved but
 *          I'm worried that the VAS will be corrupt if it doesn't perform the additional region checks   
 */
vas_allocation_t *vas_reserve(vas_t *vas, uintptr_t address, size_t size, int type) {
    if (!vas) return NULL;

    // Align
    address = MEM_ALIGN_PAGE_DESTRUCTIVE(address);
    if (size & (PAGE_SIZE-1)) size = MEM_ALIGN_PAGE(size);

    if (!RANGE_IN_RANGE(address, address+size, vas->base, vas->base + vas->size)) {
        // Outside of the VAS range!
        LOG(ERR, "Cannot reserve region outside of VAS space: %p - %p (VAS: %p - %p)\n", address, address + size, vas->base, vas->base + vas->size);
        return NULL;
    }

    // Lock the VAS
    spinlock_acquire(vas->lock);

    // Get allocation
    // As this is a reservation the kernel will allocate the pages
    vas_allocation_t *allocation = kzalloc(sizeof(vas_allocation_t));
    allocation->base = address;
    allocation->size = size;
    allocation->prot = VAS_PROT_DEFAULT;
    allocation->type = type;
    allocation->references = 1; 

    vas_node_t *node = kzalloc(sizeof(vas_node_t));
    node->alloc = allocation;

    LOG(DEBUG, "[ALLO] Allocate %p - %p\n", address, size);

    // Sanity check and to find the spot where we need to place
    vas_node_t *nn = vas->head;

    // If n exists but we can just fit it in the hole of 0 - n do that
    if (nn && ALLOC(nn)->base) {
        if (RANGE_IN_RANGE(address, address+size, 1, ALLOC(nn)->base)) {
            vas->head = node;
            node->next = nn;
            nn->prev = node;
            goto _finish_allocation;
        }
    }

    while (nn) {
        vas_allocation_t *n = ALLOC(nn);

        if (RANGE_IN_RANGE(n->base, n->base + n->size, address, address + size)) {
            LOG(WARN, "Reserving a VAS region (%p - %p) which is contained within another allocation (%p - %p)\n", n->base, n->base + n->size, address, address+size);
            LOG(WARN, "This is undefined behavior and may result in very bad consequences.\n");
            return NULL;
        }

        if (RANGE_IN_RANGE(address, address + size, n->base, n->base + n->size)) {
            LOG(WARN, "Reserving a VAS region (%p - %p) which is contained within another allocation (%p - %p)\n", address, address+size, n->base, n->base + n->size);
            LOG(WARN, "This is undefined behavior and may result in very bad consequences.\n");
            return NULL;
        }

        // If we are still iterating and n->base + n->size is greater than address, we could not find a hole. Panic.
        if (n->base + n->size > address) {
            // !!!: We have to resolve this.. how?
            kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "vas", "*** VAS \"%s\" tried to reserve %p - %p but it was taken already.\n", vas->name, address, address+size);
            __builtin_unreachable();
        }


        // There's enough space. Let's see what we have.
    
        if (!nn->next) {
            // That's all we have allocated.
            nn->next = node;
            node->prev = nn;
            vas->tail = node;
            goto _finish_allocation;
        }

        // Calculate hole width
        uintptr_t hole_base = n->base + n->size;
        size_t hole_size = nn->next->alloc->base - hole_base;

        LOG(DEBUG, "[HOLE] Hole from %016llX - %016llX\n", hole_base, hole_base + hole_size);

        // Is it enough?
        if (RANGE_IN_RANGE(address, address+size, hole_base, hole_base + hole_size)) {
            LOG(DEBUG, "[HOLE] Using hole %016llX - %016llX\n", hole_base, hole_base + hole_size);
            node->next = nn->next;
            node->prev = nn;
            nn->next = node;
            node->next->prev = node;
            goto _finish_allocation;
        }

        // No, go to next
        nn = nn->next;
    } 

    // If n is NULL, the VAS doesn't have any allocations
    if (!nn) {
        vas->head = node;
        vas->tail = node;
    }

_finish_allocation:
    vas->allocations++;
    spinlock_release(vas->lock);

    return allocation;
}

/**
 * @brief Allocate memory in a VAS
 * @param vas The VAS to reserve allocations in
 * @param size The amount of memory to allocate
 * @returns The allocation or NULL if no memory could be found
 */
vas_allocation_t *vas_allocate(vas_t *vas, size_t size) {
    if (!vas || !size) return NULL;

    // Align
    size = MEM_ALIGN_PAGE(size);

    // Acquire a VAS lock
    spinlock_acquire(vas->lock);

    // On the side of searching, calculate the highest address in use
    uintptr_t highest_address = vas->base;

    vas_allocation_t *allocation;
    vas_node_t *node;

    // Start searching for holes in the VAS
    vas_node_t *nn = vas->head;

    // Real quick - can we fit an allocation in between VAS base and the first allocation/reservation?
    if (nn && ALLOC(nn)->base) {
        uintptr_t distance = ALLOC(nn)->base - vas->base;
        
        if (distance && IN_RANGE_EXCLUSIVE(size, 0, distance)) {
            // Yes!
            allocation = kzalloc(sizeof(vas_allocation_t));
            node = kzalloc(sizeof(vas_node_t));
            node->alloc = allocation;
            allocation->base = vas->base;
            allocation->size = size;
            node->next = nn;
            allocation->prot = VAS_PROT_DEFAULT;
            allocation->references = 1;
            nn->prev = node;
            vas->head = node;
            goto _finish_allocation;
        }
    }

    while (nn) {
        vas_allocation_t *n = ALLOC(nn);
        if (n->base + n->size > highest_address) highest_address = n->base + n->size;

        // See if there's a hole
        if (!nn->next) break; // Nothing left, meaning that we've effectively reached the end
        vas_allocation_t *next = ALLOC(nn->next);

        // Calculate hole size
        size_t hole_size = next->base - (n->base + n->size);
        if (!hole_size) { nn = nn->next; if (!nn) break; n = ALLOC(nn); continue; }

        LOG(DEBUG, "[HOLE] Hole from %016llX - %016llX\n", n->base + n->size, (n->base + n->size) + hole_size);

        if (IN_RANGE(size, 1, hole_size)) {
            // Create a new allocation here
            allocation = kzalloc(sizeof(vas_allocation_t));
            node = kzalloc(sizeof(vas_node_t));
            node->alloc = allocation;
            allocation->base = n->base + n->size;
            allocation->size = size;
            node->next = nn->next;
            nn->next = node;
            node->prev = nn;
            node->next->prev = node;
            allocation->prot = VAS_PROT_DEFAULT;
            allocation->references = 1;

            goto _finish_allocation;
        }


        nn = nn->next;
    }

    // Ok, if highest_address is enough space for our allocation we're done.
    if (!IN_RANGE(highest_address + size, vas->base, vas->base + vas->size)) {
        // We don't have enough memory...
        spinlock_release(vas->lock);
        return NULL;
    }

    // Enough memory!
    allocation = kzalloc(sizeof(vas_allocation_t));
    node = kzalloc(sizeof(vas_node_t));
    node->alloc = allocation;
    allocation->base = highest_address;
    allocation->size = size;
    node->prev = vas->tail;
    allocation->prot = VAS_PROT_DEFAULT;
    allocation->references = 1;
    if (vas->tail) vas->tail->next = node;
    vas->tail = node;

    
_finish_allocation:
    vas->allocations++;
    spinlock_release(vas->lock);
    return allocation;
}

/**
 * @brief Debug to convert allocation type to string
 */
static char *vas_typeToString(int type) {
    switch (type) {
        case VAS_ALLOC_NORMAL:
            return "NORMAL ";
        case VAS_ALLOC_MMAP:
            return "MMAP   ";
        case VAS_ALLOC_MMAP_SHARE:
            return "MMAP SH";
        case VAS_ALLOC_THREAD_STACK:
            return "STACK  ";
        case VAS_ALLOC_PROG_BRK:
            return "PROGBRK";
        case VAS_ALLOC_EXECUTABLE:
            return "PROGRAM";
        default:
            return "???????";
    }
}

/**
 * @brief Free memory in a VAS
 * @param vas The VAS to free the memory in
 * @param node The node to free
 * @returns 0 on success or 1 on failure
 */
int vas_free(vas_t *vas, vas_node_t *node) {
    if (!vas || !node) return 1;
    spinlock_acquire(vas->lock);

    vas_allocation_t *allocation = ALLOC(node);

    // Setup chain now
    if (node == vas->head) {
        vas->head = node->next; 
        if (!vas->head) vas->tail = NULL;
    } else {
        if (node->prev) {
            node->prev->next = node->next;
        }

        if (node->next) {
            node->next->prev = node->prev;
        }
    }

    spinlock_acquire(&allocation->ref_lck);
    allocation->references--;
    if (!allocation->references) {
        // Drop pages
        for (uintptr_t i = allocation->base; i < allocation->base + allocation->size; i += PAGE_SIZE) {
            page_t *pg = mem_getPage(vas->dir, i, MEM_DEFAULT);
            if (pg && PAGE_PRESENT(pg)) {
                // LOG(DEBUG, "Dropped page at %016llX (frame %p)\n", i, MEM_GET_FRAME(pg));
                mem_freePage(pg);
            }
        }

        LOG(DEBUG, "Allocation dropped: [%p] [%s] %p - %p\n", allocation, vas_typeToString(allocation->type), allocation->base, allocation->base + allocation->size);

        spinlock_release(&allocation->ref_lck);
        kfree(allocation);
    } else {
        LOG(DEBUG, "Allocation dropped: [%p] [%s] %p - %p (references: %d, cow waiting: %d)\n", allocation, vas_typeToString(allocation->type), allocation->base, allocation->base + allocation->size, allocation->references, allocation->pending_cow);
        spinlock_release(&allocation->ref_lck);
    }


    // Free and return
    kfree(node);
    vas->allocations--;
    spinlock_release(vas->lock);
    return 0;
}

/**
 * @brief Get an allocation from a VAS
 * @param vas The VAS to get the allocation from
 * @param address The address to find the allocation for
 * @returns The node of the allocation or NULL if it could not be found
 */
vas_node_t *vas_get(vas_t *vas, uintptr_t address) {
    if (!vas) return NULL;

    // TODO: Is there a better algorithm for this?
    spinlock_acquire(vas->lock);

    vas_node_t *nn = vas->head;
    while (nn) {
        vas_allocation_t *n = ALLOC(nn);
        if (IN_RANGE(address, n->base, n->base + n->size) && n->base + n->size != address) {
            // Found!
            spinlock_release(vas->lock);
            return nn;
        }

        nn = nn->next;
    }

    spinlock_release(vas->lock);
    return NULL;
}

/**
 * @brief Destroy a VAS and free the memory in use
 * @param vas The VAS to destroy
 * @returns 0 on success
 */
int vas_destroy(vas_t *vas) {
    if (!vas) return 1;
    spinlock_acquire(vas->lock);

    vas_node_t *nn = vas->head;
    while (nn) {
        // TODO: mem_destroyVAS already frees memory so we need to move that to here (for CoW and more)
        vas_node_t *next = nn->next;
        vas_free(vas, nn);
        nn = next;
    }

    if (vas->dir) {
        mem_destroyVAS(vas->dir);
    }

    kfree(vas);
    spinlock_destroy(vas->lock);

    return 0;
}

/**
 * @brief Handle a VAS fault and give the memory if needed
 * @param vas The VAS
 * @param address The address the fault occurred at
 * @param size If the allocation spans over a page, this will be used as a hint for the amount to actually map in (for speed)
 * @returns 1 on fault resolution
 */
int vas_fault(vas_t *vas, uintptr_t address, size_t size) {
    if ((vas->flags & VAS_NO_COW || vas->flags & VAS_ONLY_REAL)) {
        // Can't be something we have to resolve
        return 0;
    }

    // Try to get the allocation corresponding to address
    vas_node_t *alloc_node = vas_get(vas, address);
    if (!alloc_node) return 0;
    vas_allocation_t *alloc = ALLOC(alloc_node);

    spinlock_acquire(&alloc->ref_lck);
    
    // Is it pending CoW?
    if (alloc->pending_cow) {
        // Is this the only reference to it?
        if (alloc->references <= 1) {
            // Yes, we can just map these pages as R/W
            // TODO: We map the *entire* region in, instead of doing what the other (non-CoW) part does and following the size hint given. This wastes memory in certain cases.
            alloc->pending_cow = 0;

            for (uintptr_t i = MEM_ALIGN_PAGE_DESTRUCTIVE(alloc->base); i < alloc->base + alloc->size; i += PAGE_SIZE)  {
                page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);

                // Allocate corresponding to prot flags
                int flags = (alloc->prot & VAS_PROT_WRITE ? 0 : MEM_PAGE_READONLY) | (alloc->prot & VAS_PROT_EXEC ? 0 : MEM_PAGE_NO_EXECUTE) | (vas->flags & VAS_USERMODE ? 0 : MEM_PAGE_KERNEL) | MEM_PAGE_NOALLOC;
                if (pg) mem_allocatePage(pg, flags);
            } 
        
            // LOG(DEBUG, "CoW released for %p - %p\n", alloc->base, alloc->base + alloc->size);
            spinlock_release(&alloc->ref_lck);
            return 1;
        }


        // First decrease the old allocation references
        alloc->references--;

        // Make a new allocation
        vas_allocation_t *old = alloc;
        alloc = kzalloc(sizeof(vas_allocation_t));
        alloc_node->alloc = alloc;

        alloc->base = old->base;
        alloc->prot = old->prot;
        alloc->references = 1;
        alloc->size = old->size;
        alloc->type = old->type;
        alloc->pending_cow = 0;

        // Start copying pages
        // TODO: We map the *entire* region in, instead of doing what the other (non-CoW) part does and following the size hint given. This wastes memory in certain cases.
        for (uintptr_t i = MEM_ALIGN_PAGE_DESTRUCTIVE(alloc->base); i < alloc->base + alloc->size; i += PAGE_SIZE)  {
            page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
            
            if (pg && PAGE_IS_PRESENT(pg)) {
                // Remap the old frame of the page
                uintptr_t remapped = mem_remapPhys(MEM_GET_FRAME(pg), PAGE_SIZE);

                // Allocate a new frame for the page
                uintptr_t new_frame = pmm_allocateBlock();

                // LOG(DEBUG, "CoW: Page %016llX swap frames %p - %p\n", i, MEM_GET_FRAME(pg), new_frame);
                
                // Set the new frame
                MEM_SET_FRAME(pg, new_frame);

                // Make the page R/W again
                int flags = (alloc->prot & VAS_PROT_WRITE ? 0 : MEM_PAGE_READONLY) | (alloc->prot & VAS_PROT_EXEC ? 0 : MEM_PAGE_NO_EXECUTE) | (vas->flags & VAS_USERMODE ? 0 : MEM_PAGE_KERNEL) | MEM_PAGE_NOALLOC;
                mem_allocatePage(pg, flags);

                // Copy the data
                memcpy((void*)i, (void*)remapped, PAGE_SIZE); 

                mem_unmapPhys(remapped, PAGE_SIZE);
            }
        }

        LOG(INFO, "Performed full CoW for %p - %p (now %d references remaining on this previous allocation)\n", alloc->base, alloc->base + alloc->size, old->references);
        spinlock_release(&old->ref_lck);
        return 1;
    }

    // Release lock
    spinlock_release(&alloc->ref_lck);

    // There's an allocation here - its probably lazy. How much can we map in?
    size_t actual_map_size = (alloc->size > size) ? size : alloc->size;

    // Too much?
    if (address + actual_map_size > alloc->base + alloc->size) actual_map_size = (alloc->base + alloc->size) - address;

    for (uintptr_t i = MEM_ALIGN_PAGE_DESTRUCTIVE(address); i < address + actual_map_size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_CREATE);
        
        // Allocate corresponding to prot flags
        int flags = (alloc->prot & VAS_PROT_WRITE ? 0 : MEM_PAGE_READONLY) | (alloc->prot & VAS_PROT_EXEC ? 0 : MEM_PAGE_NO_EXECUTE) | (vas->flags & VAS_USERMODE ? 0 : MEM_PAGE_KERNEL);
        if (pg) mem_allocatePage(pg, flags);
    }

    // LOG(DEBUG, "Created allocation for %p - %p\n", MEM_ALIGN_PAGE_DESTRUCTIVE(address), address+actual_map_size);
    return 1;
}

/**
 * @brief Dump VAS information
 */
void vas_dump(vas_t *vas) {
    LOG(DEBUG, "[VAS DUMP] Address space \"%s\" spans region %p - %p (%d allocations)\n", vas->name, vas->base, vas->base + vas->size, vas->allocations);
    LOG(DEBUG, "[VAS DUMP] Flags: %s %s %s %s\n", (vas->flags & VAS_USERMODE ? "USER" : "KERN"), (vas->flags & VAS_NO_COW ? "NOCOW" : "COW"), (vas->flags & VAS_ONLY_REAL ? "REAL" : "FAKE"), (vas->flags & VAS_GLOBAL ? "GLBL" : "NOTGLBL"));
    LOG(DEBUG, "[VAS DUMP] Head=%p, Tail=%p\n", vas->head, vas->tail);

    uintptr_t last_region = vas->base;
    vas_node_t *last = vas->head;
    vas_node_t *nn = vas->head;

    while (nn) {
        vas_allocation_t *n = ALLOC(nn);
        LOG(DEBUG, "[VAS DUMP]\tAllocation %p: Reserved memory region %p - %p (prev=%p, next=%p, references=%d)\n", n, n->base, n->base + n->size, nn->prev, nn->next, n->references);

        if (nn->prev != last && !(nn == vas->head)) {
            LOG(ERR, "[VAS DUMP]\t\tALLOCATION CORRUPTED: n->prev != last (%p != %p)\n", nn->prev, last);
        }


        if (n->base < last_region) {
            LOG(ERR, "[VAS DUMP]\t\tALLOCATION CORRUPTED: Below boundary line %p\n", last_region);
        }

        last_region = n->base + n->size;
        
        last = nn;
        nn = nn->next;
    }

    LOG(DEBUG, "[VAS DUMP]\t(end of allocations)\n");
}

/**
 * @brief Get an allocation node from an allocation
 * @param vas The VAS to search in 
 * @param alloc The allocation to look for
 * @returns The node or NULL
 */
vas_node_t *vas_getFromAllocation(vas_t *vas, vas_allocation_t *alloc) {
    if (!vas || !alloc) return NULL;

    vas_node_t *n = vas->head;
    spinlock_acquire(vas->lock);
    while (n) {
        if (ALLOC(n) == alloc) {
            spinlock_release(vas->lock);
            return n;
        }

        n = n->next;
    }

    spinlock_release(vas->lock);
    return NULL;
}

/**
 * @brief Create a copy of an allocation
 * @param vas The VAS to create the copy in
 * @param parent_vas The parent VAS to make a copy in
 * @param source The source allocation to copy
 * @returns A new allocation
 */
vas_allocation_t *vas_copyAllocation(vas_t *vas, vas_t *parent_vas, vas_allocation_t *source) {
    vas_allocation_t *alloc = NULL; // The allocation to insert
    vas_node_t *node = kzalloc(sizeof(vas_node_t));

    // Do we support CoW?
    if (!(vas->flags & VAS_NO_COW)) {
        // Yes, do this to be copy on write
        alloc = source;
        
        // Check to see if adding a reference would overflow
        spinlock_acquire(&alloc->ref_lck);
        if (alloc->references < UINT8_MAX) {
            // We have enough space
            alloc->references++;
            alloc->pending_cow = 1;

            // Mark all these pages as read-only
            for (uintptr_t i = alloc->base; i < alloc->base + alloc->size; i += PAGE_SIZE) {
                page_t *srcpg = mem_getPage(parent_vas->dir, i, MEM_DEFAULT);
                if (!srcpg || !PAGE_PRESENT(srcpg)) continue; 

                page_t *dstpg = mem_getPage(vas->dir, i, MEM_CREATE);

                int flags = (alloc->prot & VAS_PROT_READ ? 0 : MEM_PAGE_NOT_PRESENT) |
                    MEM_PAGE_READONLY |
                    (alloc->prot & VAS_PROT_EXEC ? 0 : MEM_PAGE_NO_EXECUTE) |
                    (vas->flags & VAS_USERMODE ? 0 : MEM_PAGE_KERNEL) | 
                    MEM_PAGE_NOALLOC;
                 
                mem_allocatePage(srcpg, flags);
                mem_allocatePage(dstpg, flags);

                MEM_SET_FRAME(dstpg, MEM_GET_FRAME(srcpg));
            }

            LOG(DEBUG, "Copied page at %016llX - %016llX (CoW for allocation %p)\n", alloc->base, alloc->base + alloc->size, alloc);
            spinlock_release(&alloc->ref_lck);
            goto _add_allocation;
        }

        spinlock_release(&alloc->ref_lck);
    }

    // No, we don't support CoW or we can't add a reference
    alloc = kzalloc(sizeof(vas_allocation_t));
    alloc->base = source->base;
    alloc->prot = source->prot;
    alloc->size = source->size;
    alloc->type = source->type;
    alloc->references = 1;

    // Iterate through allocation pages
    for (uintptr_t i = 0; i < alloc->size; i += PAGE_SIZE) {
        page_t *src = mem_getPage(parent_vas->dir, alloc->base + i, MEM_DEFAULT);
        if (!src || !PAGE_IS_PRESENT(src)) continue;  

        // Get a frame for a new page
        uintptr_t new_frame = pmm_allocateBlock();
        ref_set(new_frame >> MEM_PAGE_SHIFT, 1);

        // Copy our data to it
        uintptr_t new_frame_remapped = mem_remapPhys(new_frame, PAGE_SIZE);
        memcpy((void*)new_frame_remapped, (void*)alloc->base + i, PAGE_SIZE);
        mem_unmapPhys(new_frame_remapped, PAGE_SIZE);

        // Create the new page in the new directory
        page_t *dst = mem_getPage(vas->dir, alloc->base + i, MEM_CREATE);

        // Setup protection flags on this page
        int flags = (alloc->prot & VAS_PROT_READ ? 0 : MEM_PAGE_NOT_PRESENT) |
                        (alloc->prot & VAS_PROT_WRITE ? 0 : MEM_PAGE_READONLY) |
                        (alloc->prot & VAS_PROT_EXEC ? 0 : MEM_PAGE_NO_EXECUTE) |
                        (vas->flags & VAS_USERMODE ? 0 : MEM_PAGE_KERNEL) | 
                        MEM_PAGE_NOALLOC; 

        mem_allocatePage(dst, flags);
        MEM_SET_FRAME(dst, new_frame);

        LOG(DEBUG, "Copied page at %016llX (frame %p - %p, source references: %d, new references: %d)\n", i + alloc->base, MEM_GET_FRAME(src), MEM_GET_FRAME(dst), ref_get(MEM_GET_FRAME(src) >> MEM_PAGE_SHIFT), ref_get(MEM_GET_FRAME(dst) >> MEM_PAGE_SHIFT));
    }


_add_allocation:
    // Insert the allocation into the VAS
    // !!!: THIS ASSUMES ORDERED
    node->alloc = alloc;
    if (!vas->head) {
        vas->head = node;
        vas->tail = node;
        node->prev = NULL;
        node->next = NULL;
    } else {
        vas->tail->next = node;
        node->prev = vas->tail;
        node->next = NULL;
        vas->tail = node;
    }

    vas->allocations++;
    return alloc;
}

/**
 * @brief Clone a VAS to a new VAS
 * @param parent The parent VAS to clone from
 * @returns A new VAS from the parent VAS
 * 
 * This also sets up a new page directory with valid mappings from the parent
 */
vas_t *vas_clone(vas_t *parent) {
    if (!parent) return NULL;

    // Make a new VAS
    vas_t *vas = kzalloc(sizeof(vas_t));
    vas->name = parent->name;
    vas->base = parent->base;
    vas->allocations = 0; // Each call to vas_copyAllocation will increment this
    vas->lock = spinlock_create("vas lock");
    vas->size = parent->size;
    vas->flags = parent->flags;

    // Make a clone of the kernel directory
    vas->dir = mem_clone(mem_getKernelDirectory());

    // Copy each allocation
    vas_node_t *parent_alloc = parent->head;

    // Setup the head
    if (parent_alloc) {
        vas_copyAllocation(vas, parent, ALLOC(parent_alloc));
        
        // Get into allocation loop
        parent_alloc = parent_alloc->next;
        while (parent_alloc) {
            vas_copyAllocation(vas, parent, ALLOC(parent_alloc));
            parent_alloc = parent_alloc->next;
        }
    } else {
        vas->head = NULL;
        vas->tail = NULL;
    }

    return vas;
}