/**
 * @file hexahedron/arch/i386/mem.c
 * @brief i386 memory subsystem
 * 
 * @todo A locking subsystem NEEDS to be implemented
 * @todo Reference bitmap for pages and cloning functions, but usermode is far away.
 * @todo Map pool can use a trick from x86_64 and use 4MiB pages
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

// Standard includes
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

// Memory includes
#include <kernel/mem/mem.h>
#include <kernel/arch/i386/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/regions.h>

// General kernel includes
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/processor_data.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/pool.h>
#include <kernel/misc/spinlock.h>
#include <kernel/arch/i386/smp.h>

/* coming never */
// #define EXPERIMENTAL_COW

// Current & kernel directories
static page_t       *mem_kernelDirectory;       // Kernel page directory
                                                // TODO: We can stack allocate and align this.

// Reference counts
uint8_t     *mem_pageReferences     = NULL;     // Holds a bitmap of references

// Heap/identity map stuff
uintptr_t    mem_kernelHeap;                    // Location of the kernel heap in memory
uintptr_t    mem_identityMapCacheSize;          // Size of our actual identity map (it is basically a cache)
pool_t       *mem_mapPool = NULL;               // Identity map pool

// Regions
uintptr_t       mem_mmioRegion      = MEM_MMIO_REGION;     // Memory-mapped I/O region
uintptr_t       mem_driverRegion    = MEM_DRIVER_REGION;   // Driver region
uintptr_t       mem_dmaRegion       = MEM_DMA_REGION;      // DMA region

// Spinlocks (stack-allocated - no spinlock for ID map is required as pool system handles that)
static spinlock_t heap_lock = { 0 };
static spinlock_t mmio_lock = { 0 };
static spinlock_t driver_lock = { 0 };
static spinlock_t dma_lock = { 0 };


/**
 * @brief Get the current position of the kernel heap
 * @returns The current position of the kernel heap
 */
uintptr_t mem_getKernelHeap() {
    return mem_kernelHeap;
}

/**
 * @brief Invalidate a page in the TLB
 * @param addr The address of the page 
 * @warning This function is only to be used when removing P-V mappings. Just free the page if it's identity.
 */
static inline void mem_invalidatePage(uintptr_t addr) {
    asm volatile ("invlpg (%0)" :: "r"(addr) : "memory");
    smp_tlbShootdown(addr);
} 

/**
 * @brief Load new value into the PDBR
 * @param pagedir The address of the page directory to load
 */
static inline void mem_load_pdbr(uintptr_t addr) {
    asm volatile ("mov %0, %%cr3" :: "r"(addr));
}

/**
 * @brief Enable/disable paging
 * @param status Enable/disable
 */
void mem_setPaging(bool status) {
    if (status) {
        // Turn on paging
        uint32_t cr0, cr4;

        asm volatile ("mov %%cr4, %0" : "=r"(cr4));
        cr4 = cr4 & 0xFFFFFFEF; // Clear PSE bit
        asm volatile ("mov %0, %%cr4" :: "r"(cr4));

        asm volatile ("mov %%cr0, %0" : "=r"(cr0));
        cr0 = cr0 | 0x80010001; // Enable paging
        cr0 = cr0 & 0x9FFFFFFF;
        asm volatile ("mov %0, %%cr0" :: "r"(cr0));
    } else {
        uint32_t cr0;

        asm volatile ("mov %%cr0, %0" : "=r"(cr0));
        cr0 = cr0 & ~CR0_PG_BIT; // Disable paging
        asm volatile ("mov %0, %%cr4" :: "r"(cr0));
    }
}

/**
 * @brief Get the current page directory
 */
page_t *mem_getCurrentDirectory() {
    return current_cpu->current_dir;
}

/**
 * @brief Get the kernel page directory
 */
page_t *mem_getKernelDirectory() {
    return mem_kernelDirectory;
}

/**
 * @brief Switch the memory management directory
 * @param pagedir The virtual address of the page directory to switch to, or NULL for the kernel region
 * 
 * @warning If bootstrapping, it is best to load this yourself. This method may rely on things like mem_getPhysicalAddress
 * 
 * @returns -EINVAL on invalid, 0 on success.
 */
int mem_switchDirectory(page_t *pagedir) {
    if (!pagedir) pagedir = mem_getKernelDirectory();
    if (current_cpu->current_dir == pagedir) return 0; // No need to waste time


    // Try to figure out what physical address we should use
    // !!!: This is weird, not standardized.
    if ((uintptr_t)pagedir > MEM_PHYSMEM_CACHE_REGION && (uintptr_t)pagedir < MEM_PHYSMEM_CACHE_REGION + MEM_PHYSMEM_CACHE_SIZE) {
        // In cached region, just load it anyways
        mem_load_pdbr((uintptr_t)pagedir & ~MEM_PHYSMEM_CACHE_REGION); 
    } else {
        // Not in cache, try to get physical address
        if (!current_cpu->current_dir) return -ENOTSUP;

        // Get physical
        uintptr_t phys = mem_getPhysicalAddress(NULL, (uintptr_t)pagedir);
        if (phys == 0x0) return -EINVAL;

        // Load PDBR
        mem_load_pdbr(phys);
    }

    // Load into current directory
    current_cpu->current_dir = pagedir;

    return 0;
}

/**
 * @brief Increment a page refcount
 * @param page The page to increment reference counts of
 * @returns The number of reference counts or 0 if maximum is reached
 */
int mem_incrementPageReference(page_t *page) {
    if (!page) return 0; // ???
    if (!page->bits.present) {
        dprintf(ERR, "Tried incrementing reference count on non-present page\n");
        return 0;
    }

    // First get the index in refcounts of the frame
    uintptr_t idx = page->bits.address;
    if (mem_pageReferences[idx] == UINT8_MAX) {
        // We're too high, return 0 and hope they make a copy of the page
        return 0; 
    }

    mem_pageReferences[idx]++;
    return mem_pageReferences[idx];
}

/**
 * @brief Decrement a page refcount
 * @param page The page to decrement the reference count of
 * @returns The number of reference counts. Panicks if 0
 */
int mem_decrementPageReference(page_t *page) {
    if (!page) return 0; // ???
    if (!page->bits.present) {
        dprintf(ERR, "Tried decrementing reference count on non-present page\n");
        return 0;
    }

    // First get the index in refcounts of the frame
    uintptr_t idx = page->bits.address;
    if (mem_pageReferences[idx] == 0) {
        // Bail out!
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "pageref", "*** Tried to release reference on page with 0 references (bug)\n");
        __builtin_unreachable();
    }

    mem_pageReferences[idx]--;
    return mem_pageReferences[idx];
}


/**
 * @brief Create a new, completely blank virtual address space
 * @returns A pointer to the VAS
 */
page_t *mem_createVAS() {
    page_t *vas = (page_t*)mem_remapPhys(pmm_allocateBlock(), PMM_BLOCK_SIZE);
    memset((void*)vas, 0, PMM_BLOCK_SIZE);
    return vas;
}

/**
 * @brief Destroys and frees the memory of a VAS
 * @param vas The VAS to destroy
 * 
 * @warning Make sure the VAS being freed isn't the current one selected
 */
void mem_destroyVAS(page_t *vas) {
    dprintf(DEBUG, "UNIMPL: Destroy VAS @ %p\n", vas);
}


/**
 * @brief Maybe copy a usermode page
 * @param src_pt The source page table
 * @param dest_pt The destination page table
 * 
 * @ref https://github.com/klange/toaruos/blob/master/kernel/arch/x86_64/mmu.c
 * 
 * @returns The block address of the table
 */
static void mem_copyUserPage(page_t *src, page_t *dest) {

#ifdef EXPERIMENTAL_COW

    // Check if the source page is writable
    if (src->bits.rw) {
        // It is, initialize reference counts for the page's frame
        if (mem_pageReferences[src->bits.address]) {
            // There's already references??
            kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "CoW", "*** Source page already has references\n");
            __builtin_unreachable();
        }

        // 2 references
        mem_pageReferences[src->bits.address] = 2;

        // Now what we do is mark the source AND destination page as R/O and set them both to have a CoW pending
        // Any writes will trigger a page fault, which our handler will detect and auto handle CoW
        src->bits.rw = 0;
        src->bits.cow = 1;

        // Raw copy to destination
        dest->data = src->data;

        // All done!
        // TODO: Invalidate the page with TLB shootdown
        dprintf(WARN, "IMPLEMENT TLB SHOOTDOWN\n");
        return;
    }

    // It's not writable. Can we add a new reference?
    if (mem_incrementPageReference(src) == 0) {
        // There are too many reference counts. We need to create a copy of the page.
        // We can do this by mapping the old page into memory, creating a new page, copying contents, and then done
        uintptr_t src_frame = mem_remapPhys(MEM_GET_FRAME(src), PAGE_SIZE);
        uintptr_t dest_frame_block = pmm_allocateBlock();
        uintptr_t dest_frame = mem_remapPhys(dest_frame_block, PAGE_SIZE);
        memcpy((void*)dest_frame, (void*)src_frame, PAGE_SIZE);

        // Setup bits
        dest->data = src->data;
        MEM_SET_FRAME(dest, dest_frame_block);
        dest->bits.cow = 0; // Not CoW

        mem_unmapPhys(dest_frame, PAGE_SIZE);
        mem_unmapPhys(src_frame, PAGE_SIZE);
        return;
    }

    // Yes, we can. Raw copy and return
    dest->data = src->data;

#else

    // Copy the page
    uintptr_t src_frame = mem_remapPhys(MEM_GET_FRAME(src), PAGE_SIZE);
    uintptr_t dest_frame_block = pmm_allocateBlock();
    uintptr_t dest_frame = mem_remapPhys(dest_frame_block, PAGE_SIZE);
    memcpy((void*)dest_frame, (void*)src_frame, PAGE_SIZE);

    // Setup bits
    dest->data = src->data;
    MEM_SET_FRAME(dest, dest_frame_block);
    dest->bits.cow = 0; // Not CoW

    mem_unmapPhys(dest_frame, PAGE_SIZE);
    mem_unmapPhys(src_frame, PAGE_SIZE);
    return;
#endif
}


/**
 * @brief Clone a page directory.
 * 
 * This is a full PROPER page directory clone.
 * This function does it properly and clones the page directory, its tables, and their respective entries fully.
 * It also has the option to do CoW on usermode pages
 * 
 * @param dir The source page directory. Keep as NULL to clone the current page directory.
 * @returns The page directory on success
 */
page_t *mem_clone(page_t *dir) {
    if (!dir) dir = mem_getCurrentDirectory();

    // Get our return directory
    page_t *dest = mem_createVAS();

    // Now start copying PDEs
    for (int pde = 0; pde < 1024; pde++) {
        page_t *src_pde = &dir[pde];
        if (!(src_pde->bits.present)) continue; // PDE isn't present

        if (pde == MEM_RECURSIVE_PAGING_ENTRY) continue;

        // Construct a new table and add it to our output
        uintptr_t dest_pt_block = pmm_allocateBlock();
        page_t *dest_pt = (page_t*)mem_remapPhys(dest_pt_block, PMM_BLOCK_SIZE);
        memset((void*)dest_pt, 0, PMM_BLOCK_SIZE);

        // Get the PDE in our new VAS and set it up to point to new_pt
        page_t *dest_pde = &dest[pde];
        
        // Setup the bits
        dest_pde->data = src_pde->data; // Do a raw copy first
        MEM_SET_FRAME(dest_pde, dest_pt_block);

        // Now get the source PT
        page_t *src_pt = (page_t*)mem_remapPhys((uintptr_t)MEM_GET_FRAME(src_pde), PMM_BLOCK_SIZE);

        for (int pte = 0; pte < 1024; pte++) {
            page_t *src_pte = &src_pt[pte];
            if (!(src_pte->bits.present)) continue; // Not present

            page_t *dest_pte = &dest_pt[pte];

            // Is it a usermode page? We need to do CoW in that case
            if (src_pte->bits.usermode) {
                mem_copyUserPage(src_pte, dest_pte);
            } else {
                // Just do raw copy
                dest_pte->data = src_pte->data;
            }
        }

        // Cleanup and unmap
        mem_unmapPhys((uintptr_t)src_pt, PMM_BLOCK_SIZE);
        mem_unmapPhys((uintptr_t)dest_pt, PMM_BLOCK_SIZE);
    }

    // Remember to recurse!
    dest[MEM_RECURSIVE_PAGING_ENTRY].bits.present = 1;
    dest[MEM_RECURSIVE_PAGING_ENTRY].bits.rw = 1;
    MEM_SET_FRAME((&dest[MEM_RECURSIVE_PAGING_ENTRY]), mem_getPhysicalAddress(NULL, (uintptr_t)dest));

    return dest;
}

/**
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) {    
    if (frame_address + size < mem_identityMapCacheSize) {
        return frame_address | MEM_PHYSMEM_CACHE_REGION;
    }

    if (mem_mapPool == NULL) {
        // Initialize the map pool! We'll allocate a pool to the address.
        // !!!: There is a potential for a disaster if mem_getPage tries to remap phys. and the pool hasn't been initialized.  
        // !!!: Luckily this system is abstracted enough that we can fix this, hopefully.
        // !!!: However if the allocator hasn't been initialized, we are so screwed.
        mem_mapPool = pool_create("map pool", PAGE_SIZE, MEM_PHYSMEM_MAP_SIZE, MEM_PHYSMEM_MAP_REGION, POOL_DEFAULT);
        dprintf(INFO, "Physical memory identity map pool created (0x%x - 0x%x)\n", MEM_PHYSMEM_MAP_REGION, MEM_PHYSMEM_MAP_REGION + MEM_PHYSMEM_MAP_SIZE);
    }

    uintptr_t offset = 0; // This is the offset which will be added to the allocated chunks (in order to result in a frame address equal to the one passed in)

    if (size % PAGE_SIZE != 0) {
        size = MEM_ALIGN_PAGE(size);
        // kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "*** Bad size to remapPhys: 0x%x\n", size);
    }

    if (frame_address % PAGE_SIZE != 0) {
        // We know that frame_address + size will overflow into more pages (e.g. frame address 0x7FE1900 + 0x1000 actually requires 2 pages for 0x7FE1900 - 0x7FE2000)
        // !!!: this is wasteful and stupid, just like this whole system.
        offset = frame_address & 0xFFF;
        frame_address = frame_address & ~0xFFF;
        size += PAGE_SIZE;

        // kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "*** Bad frame to remapPhys: 0x%x\n", frame_address);
    }
    
    // Now try to get a pool address
    uintptr_t start_addr = pool_allocateChunks(mem_mapPool, size / PAGE_SIZE);
    if (start_addr == (uintptr_t)NULL) {
        // We've run out of space in the identity map. That's not great!
        // There should be a system in place to prevent this - it should just trigger a generic OOM error but this is also here to prevent overruns.
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Too much physical memory is in use. Reached the maximum size of the identity mapped region (call 0x%x size 0x%x).\n", frame_address, size);
        __builtin_unreachable();
    }

    for (uintptr_t i = frame_address; i < frame_address + size; i += PAGE_SIZE) {
        // Do this manually
        mem_mapAddress(NULL, i, start_addr + (i - frame_address), MEM_PAGE_KERNEL);
    }

    return start_addr + offset;
}

/**
 * @brief Unmap a PMM address in the identity mapped region
 * @param frame_address The address of the frame to unmap, as returned by @c mem_remapPhys
 * @param size The size of the frame to unmap
 */
void mem_unmapPhys(uintptr_t frame_address, uintptr_t size) {
    if (frame_address < MEM_PHYSMEM_CACHE_REGION) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "*** 0x%x < 0x%x\n", frame_address, MEM_PHYSMEM_CACHE_REGION);
    }

    if ((frame_address - MEM_PHYSMEM_CACHE_REGION) + size < mem_identityMapCacheSize) {
        return; // No work to be done. It's in the cache.
    }

    if (size % PAGE_SIZE != 0) {
        size = MEM_ALIGN_PAGE(size);
    }

    if (frame_address % PAGE_SIZE != 0) {
        frame_address = frame_address & ~0xFFF;
        size += PAGE_SIZE;
    }

    // See if we can just free those chunks. Because mem_remapPhys doesn't actually use pmm_allocateBlock,
    // we have no need to mess with pages
    pool_freeChunks(mem_mapPool, frame_address, size / PAGE_SIZE);
}

/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr) {
    page_t *directory = (dir == NULL) ? current_cpu->current_dir : dir;
    
    // Create an offset
    uintptr_t offset = (virtaddr & 0xFFF);

    // Get the directory entry and its corresponding table
    uintptr_t addr = (virtaddr % PAGE_SIZE == 0) ? virtaddr : (virtaddr & ~0xFFF);
    page_t *pde = &(directory[MEM_PAGEDIR_INDEX(addr)]);
    if (pde->bits.present == 0) {
        // The PDE wasn't present
        return (uintptr_t)NULL;
    }

    // Remember to remap any frames to that identity map area.
    page_t *table = (page_t*)((dir) ? (mem_remapPhys(MEM_GET_FRAME(pde), PMM_BLOCK_SIZE)) : (MEM_PAGE_TABLE(MEM_PAGEDIR_INDEX(addr)))); 
    page_t *pte = &(table[MEM_PAGETBL_INDEX(addr)]);
    
    if (dir) mem_unmapPhys((uintptr_t)table, PMM_BLOCK_SIZE);

    return MEM_GET_FRAME(pte) + offset;
}

/**
 * @brief Map a physical address to a virtual address
 * 
 * @param dir The directory to map them in (leave blank for current)
 * @param phys The physical address
 * @param virt The virtual address
 * @param flags Additional flags to use (e.g. MEM_KERNEL)
 */
void mem_mapAddress(page_t *dir, uintptr_t phys, uintptr_t virt, int flags) {
    page_t *directory = (dir == NULL) ? current_cpu->current_dir : dir;

    // Get the page
    page_t *page = mem_getPage(directory, virt, MEM_CREATE);

    // "Allocate" it but don't set a frame, instead set it to phys
    mem_allocatePage(page, MEM_PAGE_NOALLOC | flags);
    MEM_SET_FRAME(page, phys);
}




/**
 * @brief Returns the page entry requested
 * @param dir The directory to search. Specify NULL for current directory
 * @param address The virtual address of the page (will be aligned for you if not aligned)
 * @param flags The flags of the page to look for
 * 
 * @warning Specifying MEM_CREATE will only create needed structures, it will NOT allocate the page!
 *          Please use a function such as mem_allocatePage to do that.
 */
page_t *mem_getPage(page_t *dir, uintptr_t address, uintptr_t flags) {
    uintptr_t addr = (address % PAGE_SIZE != 0) ? MEM_ALIGN_PAGE_DESTRUCTIVE(address) : address;

    page_t *directory = (dir) ? dir : current_cpu->current_dir;

    // Page addresses are divided into 3 parts:
    // - The index of the PDE (bits 22-31)
    // - The index of the PTE (bits 12-21)
    // - The page offset (bits 0-11)

    // Check if the PDE is present
    page_t *pde = &(directory[MEM_PAGEDIR_INDEX(addr)]);
    if (!pde->bits.present) {
        if (!flags & MEM_CREATE) goto bad_page;

        // Allocate a new PDE and zero it
        uintptr_t block = pmm_allocateBlock();
        
        // Setup the bits in the directory index
        pde->bits.present = 1;
        pde->bits.rw = 1;
        pde->bits.usermode = 1; // !!!: Not upholding security 
        MEM_SET_FRAME(pde, block);

        // If the user specified to use current page directory, we don't need to map it. Just use recursive paging.
        if (dir) {
            uintptr_t block_remap = mem_remapPhys(block, PMM_BLOCK_SIZE);
            memset((void*)block_remap, 0, PMM_BLOCK_SIZE);
            mem_unmapPhys(block_remap, PMM_BLOCK_SIZE);
        } else {
            // Use faster recursive paging!
            memset((void*)MEM_PAGE_TABLE(MEM_PAGEDIR_INDEX(addr)), 0, sizeof(page_t) * 1024);
        }
    }

    // Calculate the table index (complex bc MEM_GET_FRAME is for pointers, and I'm lazy)
    page_t *table;
    if (dir) {
        table = (page_t*)mem_remapPhys((uintptr_t)(directory[MEM_PAGEDIR_INDEX(addr)].bits.address << MEM_PAGE_SHIFT), PMM_BLOCK_SIZE);
    } else {
        table = (page_t*)MEM_PAGE_TABLE(MEM_PAGEDIR_INDEX(addr));
    }

    page_t *ret = &(table[MEM_PAGETBL_INDEX(addr)]);
    if (dir) mem_unmapPhys((uintptr_t)table, PMM_BLOCK_SIZE);

    // Return the page
    return ret;

bad_page:
    return NULL;
}

/**
 * @brief Allocate a page using the physical memory manager
 * 
 * @param page The page object to use. Can be obtained with mem_getPage
 * @param flags The flags to follow when setting up the page
 * 
 * @note You can also use this function to set bits of a specific page - just specify @c MEM_NOALLOC in @p flags.
 * @warning The function will automatically allocate a PMM block if NOALLOC isn't specified
 */
void mem_allocatePage(page_t *page, uintptr_t flags) {
    if (flags & MEM_PAGE_FREE) {
        // Just free the page
        mem_freePage(page);
        return;
    }

    if (!(flags & MEM_PAGE_NOALLOC)) {
        // There isn't a frame configured, and the user wants to allocate one.
        uintptr_t block = pmm_allocateBlock();
        MEM_SET_FRAME(page, block);
    }

    // Configure page bits
    page->bits.present          = (flags & MEM_PAGE_NOT_PRESENT) ? 0 : 1;
    page->bits.rw               = (flags & MEM_PAGE_READONLY) ? 0 : 1;
    page->bits.usermode         = (flags & MEM_PAGE_KERNEL) ? 0 : 1;
    page->bits.writethrough     = (flags & MEM_PAGE_WRITETHROUGH) ? 1 : 0;
    page->bits.cache_disable    = (flags & MEM_PAGE_NOT_CACHEABLE) ? 1 : 0;
}

/**
 * @brief Free a page
 * 
 * @param page The page to free
 */
void mem_freePage(page_t *page) {
    if (!page) return;

    // Mark the page as not present
    page->bits.present = 0;
    page->bits.rw = 0;
    page->bits.usermode = 0;
    
    // Free the block
    pmm_freeBlock(MEM_GET_FRAME(page));
    MEM_SET_FRAME(page, 0x0);
}

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will setup the memory map and prepare tables.
 * It expects you to provide the highest kernel adress that is valid.
 * 
 * @param high_address The ending address of the kernel including all data structures copied.
 */
void mem_init(uintptr_t high_address) {
    if (!high_address) kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "mem");
    mem_kernelHeap = MEM_ALIGN_PAGE(high_address);

    // Get ourselves a page directory
    // !!!: Is this okay? Do we need to again put things in data structures?
    page_t *page_directory = (page_t*)pmm_allocateBlock();
    memset(page_directory, 0, PMM_BLOCK_SIZE);

    
    // We only have access to 4GB of VAS because of 32-bit protected mode
    // If and when PAE support is implemented into the kernel, we'd get a little more but some machines
    // have much more than that anyways. We need to access PMM memory or else we'll fault everything out of existence.
    // Hexahedron uses a memory map that has mapped PMM memory accessible through a range,
    // which is limited to something like 786 MB (see arch/i386/mem.h)

    // !!!: This sucks. The cache implementation is very finnicky and addresses aren't properly mapped. This entire system needs a full overhaul.
    // !!!: Calculating frame_bytes by multiplying max blocks by PMM_BLOCK_SIZE doesn't give the highest address in memory.

    size_t frame_bytes = pmm_getMaximumBlocks() * PMM_BLOCK_SIZE;

    if (frame_bytes > MEM_PHYSMEM_CACHE_SIZE) {
        dprintf(WARN, "Too much memory in PMM bitmap. Maximum allowed memory size is %i KB and found %i KB - limiting size\n", MEM_PHYSMEM_CACHE_SIZE / 1024, frame_bytes / 1024);
        frame_bytes = MEM_PHYSMEM_CACHE_SIZE;
    }


    size_t frame_pages = (frame_bytes >> MEM_PAGE_SHIFT);

    // Update size
    mem_identityMapCacheSize = frame_bytes; 

    // Identity map!
    uintptr_t frame = 0x0;
    uintptr_t table_frame = 0x0;
    int loop_cycles = (frame_pages + 1024) / 1024;
    int pages_mapped = 0;

    for (int i = 0; i < loop_cycles; i++) {
        page_t *page_table = (page_t*)pmm_allocateBlock();
        memset(page_table, 0, PMM_BLOCK_SIZE);

        for (int pg = 0; pg < 1024; pg++) {
            page_t page = {.data = 0};
            page.bits.present = 1;
            page.bits.rw = 1;
            page.bits.address = (frame >> MEM_PAGE_SHIFT);
            
            page_table[MEM_PAGETBL_INDEX(frame + MEM_PHYSMEM_CACHE_REGION)] = page;

            pages_mapped++;
            if (pages_mapped == (int)(frame_pages)) break;
            frame += 0x1000;
        }
        
        // Create a PDE
        page_t *pde = &(page_directory[MEM_PAGEDIR_INDEX(table_frame + MEM_PHYSMEM_CACHE_REGION)]);
        pde->bits.present = 1;
        pde->bits.rw = 1;
        MEM_SET_FRAME(pde, page_table);

        table_frame += 0x1000 * 1024;

        if (pages_mapped == (int)frame_pages) break;
    }

    uintptr_t heap_start_aligned = ((uintptr_t)mem_kernelHeap + 0xFFF) & ~0xFFF;
    uintptr_t kernel_pages = (uintptr_t)heap_start_aligned >> MEM_PAGE_SHIFT;

    // Calculate cycles and reset values
    loop_cycles = (kernel_pages + 1024) / 1024;
    pages_mapped = 0;

    frame = 0x0;
    table_frame = 0x0;

    for (int i = 0; i < loop_cycles; i++) {
        page_t *page_table = (page_t*)pmm_allocateBlock();
        memset(page_table, 0, PMM_BLOCK_SIZE);

        for (int pg = 0; pg < 1024; pg++) {
            page_t page = {.data = 0};
            page.bits.present = 1;
            page.bits.rw = 1;
            page.bits.address = (frame >> MEM_PAGE_SHIFT);
            
            page_table[MEM_PAGETBL_INDEX(frame)] = page;

            pages_mapped++;
            if (pages_mapped == (int)(kernel_pages)) break;

            frame += 0x1000;
        }
        
        // Create a PDE
        page_t *pde = &(page_directory[MEM_PAGEDIR_INDEX(table_frame)]);
        pde->bits.present = 1;
        pde->bits.rw = 1;
        MEM_SET_FRAME(pde, page_table);

        table_frame += 0x1000 * 1024;

        if (pages_mapped == (int)kernel_pages) break;
    }

    // Setup recursive paging
    page_directory[1023].bits.present = 1;
    page_directory[1023].bits.rw = 1;
    MEM_SET_FRAME((&page_directory[1023]), page_directory);

    // All done mapping for now. The memory map should look something like this:
    // 0x00000000 - 0x00400000 is kernel code (-RW)
    // 0xB0000000 - 0xBFFFFFFF is PMM mapped memory (URW)
    // !! PMM mapped memory is exposed. Very bad.

    dprintf(INFO, "Finished creating memory map.\n");
    dprintf(DEBUG, "\tKernel code is from 0x0 - 0x%x\n", high_address);
    dprintf(DEBUG, "\tKernel heap will begin at 0x%x\n", mem_kernelHeap);

    // !!!: BAD!!!! WHY ARE WE GIVING IT A PHYSMEM REGION?? WHAT IF SOMEHOW THIS ISNT CACHED?
    if ((uintptr_t)mem_kernelDirectory > MEM_PHYSMEM_CACHE_SIZE) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** BAD CODING DECISIONS HAVE LED TO THIS - Kernel directory is not in cache! Report this as a bug!!\n");
    }

    mem_kernelDirectory = (page_t*)((uintptr_t)page_directory | MEM_PHYSMEM_CACHE_REGION);
    mem_load_pdbr((uintptr_t)page_directory); // Load PMM block only (CR3 expects a physical address)
    current_cpu->current_dir = mem_kernelDirectory;
    mem_setPaging(true);

    // Make space for reference counts in kernel heap
    // Reference counts will be initialized when a user PTE is copied.
    // NOTE: This has to be done here because mem_sbrk calls mem_getPage which in turn can wrap into mem_remapPhys' map pool system
    size_t refcount_bytes = frame_bytes >> MEM_PAGE_SHIFT;  // One byte per page
    mem_pageReferences = (uint8_t*)mem_sbrk((refcount_bytes & 0xFFF) ? MEM_ALIGN_PAGE(refcount_bytes) : refcount_bytes);
    memset(mem_pageReferences, 0, refcount_bytes);

    // Initialize regions
    mem_regionsInitialize();


    dprintf(INFO, "Memory system online and enabled.\n");
}

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b) {
    // Sanity checks
    if (!mem_kernelHeap) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "Heap not yet ready\n");
        __builtin_unreachable();
    }

    // Passing b as 0 means they just want the current heap address
    if (!b) return mem_kernelHeap;

    // Make sure the passed integer is a multiple of PAGE_SIZE
    if (b % PAGE_SIZE != 0) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "Heap size expansion must be a multiple of 0x%x\n", PAGE_SIZE);
        __builtin_unreachable();
    }


    // Trying to shrink the heap?
    if (b < 0) {
        size_t positive = (size_t)(b * -1);

        uintptr_t ret = mem_getKernelHeap();
        mem_free(ret - positive, positive, MEM_ALLOC_HEAP);
        return ret;
    }

    // Else just use mem_allocate
    return mem_allocate(0x0, (size_t)(b), MEM_ALLOC_HEAP, MEM_PAGE_KERNEL);
}

/**
 * @brief Allocate a region of memory
 * @param start The starting virtual address (OPTIONAL IF YOU SPECIFY MEM_ALLOC_HEAP)
 * @param size How much memory to allocate (will be aligned)
 * @param flags Flags to use for @c mem_allocate (e.g. MEM_ALLOC_CONTIGUOUS)
 * @param page_flags Flags to use for @c mem_allocatePage (e.g. MEM_PAGE_KERNEL)
 * @returns Pointer to the new region of memory or 0x0 on failure
 * 
 * @note This is a newer addition to the memory subsystem. It may seem like it doesn't fit in. It probably doesn't. Memory rewrite is on the todo list
 */
uintptr_t mem_allocate(uintptr_t start, size_t size, uintptr_t flags, uintptr_t page_flags) {
    if (!size) return start;

    // Save variables
    uintptr_t start_original = start;
    size_t size_original = size;

    // Sanity checks
    if (start == 0x0 && !(flags & MEM_ALLOC_HEAP)) {
        dprintf(WARN, "Cannot allocate to 0x0 (MEM_ALLOC_HEAP not specified)\n");
        goto _error;
    }

    // If we're trying to allocate from the heap, update variables
    if (flags & MEM_ALLOC_HEAP) {
        // Position at start of heap
        start = mem_getKernelHeap();

        // We return start_original
        start_original = start;

        // Add MEM_PAGE_KERNEL to prevent leaking heap memory
        page_flags |= MEM_PAGE_KERNEL;
    }

    // Align start
    uintptr_t size_actual = size + (start & 0xFFF);
    start &= ~0xFFF;
    if (size_actual & 0xFFF) size_actual = MEM_ALIGN_PAGE(size_actual);

    // If we're doing fragile allocation we need to make sure none of the pages are in use
    if (flags & MEM_ALLOC_FRAGILE) {
        for (uintptr_t i = start; i < start + size_actual; i += PAGE_SIZE) {
            page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
            if (pg) {
                dprintf(ERR, "Fragile allocation failed - found present page at %p\n", i);
                goto _error;
            }
        }
    }

    // Start allocation
    if (flags & MEM_ALLOC_HEAP) spinlock_acquire(&heap_lock);


    // Are we contiguous?
    uintptr_t contig = 0x0;
    if (flags & MEM_ALLOC_CONTIGUOUS) contig = pmm_allocateBlocks(size_actual / PMM_BLOCK_SIZE);

    // Now actually start mapping
    for (uintptr_t i = start; i < start + size_actual; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_CREATE); 
        if (!pg) {
            dprintf(ERR, "Could not get page at %p\n", i);
            goto _error;
        }

        // We've got the page - did they want contiguous?
        if (flags & MEM_ALLOC_CONTIGUOUS) {
            mem_allocatePage(pg, page_flags | MEM_PAGE_NOALLOC);
            MEM_SET_FRAME(pg, (contig + (i-start)));
        } else {
            mem_allocatePage(pg, page_flags);
        }
    }

    // Done, update heap if needed
    if (flags & MEM_ALLOC_HEAP) {
        mem_kernelHeap += size_actual;
        spinlock_release(&heap_lock);
    }

    // All done
    return start_original;

_error:
    // Critical?
    if (flags & MEM_ALLOC_CRITICAL) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Critical allocation failed - could not allocate %d bytes in %p (flags %d page flags %d)\n", size_original, start_original, flags, page_flags);
        __builtin_unreachable();
    }

    return 0x0;
}

/**
 * @brief Free a region of memory
 * @param start The starting virtual address (must be specified)
 * @param size How much memory was allocated (will be aligned)
 * @param flags Flags to use for @c mem_free (e.g. MEM_ALLOC_HEAP)
 * @note Most flags do not affect @c mem_free
 */
void mem_free(uintptr_t start, size_t size, uintptr_t flags) {
    if (!start || !size) return;

    // Align start
    uintptr_t size_actual = size + (start & 0xFFF);
    start &= ~0xFFF;
    size_actual = MEM_ALIGN_PAGE(size_actual);

    // If we're getting from heap, grab lock
    if (flags & MEM_ALLOC_HEAP) spinlock_acquire(&heap_lock);

    // Start freeing
    for (uintptr_t i = start; i < start + size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
        if (!pg) {
            dprintf(WARN, "Tried to free page %p but it is not present (?)\n", i);
        }
        
        mem_allocatePage(pg, MEM_PAGE_FREE);
    }

    // All done
    if (flags & MEM_ALLOC_HEAP) {
        mem_kernelHeap -= size;
        spinlock_release(&heap_lock);
    }
}

/**
 * @brief Validate a specific pointer in memory
 * @param ptr The pointer you wish to validate
 * @param flags The flags the pointer must meet - by default, kernel mode and R/W (see PTR_...)
 * @returns 1 on a valid pointer
 */
int mem_validate(void *ptr, unsigned int flags) {
    // Get page of pointer
    page_t *pg = mem_getPage(NULL, (uintptr_t)ptr, MEM_DEFAULT);
    if (!pg) return 0;

    // Validate flags
    int valid = 1;
    if (flags & PTR_STRICT) {
        if (flags & PTR_USER && !(pg->bits.usermode)) valid = 0;
        if (flags & PTR_READONLY && pg->bits.rw) valid = 0;
    } else {
        if (pg->bits.usermode && !(flags & PTR_USER)) valid = 0;
        if (!(pg->bits.rw) && !(flags & PTR_READONLY)) valid = 0;
    }

    return valid;
}