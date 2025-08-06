/**
 * @file hexahedron/arch/x86_64/mem.c
 * @brief Memory management functions for x86_64
 * 
 * @warning A lot of functions in this file do not conform to the "standard" of unmapping
 *          physical addresses after you have finished. This is fine for now, but may have issues
 *          later.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/mem.h>
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/registers.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/regions.h>
#include <kernel/mem/pmm.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/misc/spinlock.h>
#include <kernel/task/process.h>

#include <stdint.h>
#include <string.h>

// Heap/MMIO/driver space
uintptr_t mem_kernelHeap                = 0xAAAAAAAAAAAAAAAA;   // Kernel heap
uintptr_t mem_driverRegion              = MEM_DRIVER_REGION;    // Driver space
uintptr_t mem_dmaRegion                 = MEM_DMA_REGION;       // DMA region
uintptr_t mem_mmioRegion                = MEM_MMIO_REGION;      // MMIO region

// Spinlocks
static spinlock_t heap_lock = { 0 };
static spinlock_t driver_lock = { 0 };
static spinlock_t dma_lock = { 0 };
static spinlock_t mmio_lock = { 0 };

// Stub variables (for debugger)
uintptr_t mem_mapPool                   = 0xAAAAAAAAAAAAAAAA;
uintptr_t mem_identityMapCacheSize      = 0xAAAAAAAAAAAAAAAA;   

// Whether to use 5-level paging (TODO)
static int mem_use5LevelPaging = 0;

// TODO: i386 doesn't use this method of having preallocated page structures... maybe we should use it there?

// Base page layout - loader uses this
page_t mem_kernelPML[3][512] __attribute__((aligned(PAGE_SIZE))) = {0};

// Low base PDPT/PD/PT (identity mapping space for kernel/other stuff)
page_t mem_identityBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0}; 
page_t mem_identityBasePD[128][512] __attribute__((aligned(PAGE_SIZE))) = {0};

// High base PDPT/PD/PT (identity mapping space for anything)
page_t mem_highBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_highBasePD[512] __attribute__((aligned(PAGE_SIZE))) = {0}; // If we're already using 2MiB paging, why bother with PTs? 
page_t mem_highBasePTs[512*12] __attribute__((aligned(PAGE_SIZE))) = {0};

// Heap PDPT/PD/PT
page_t mem_heapBasePDPT[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_heapBasePD[512] __attribute__((aligned(PAGE_SIZE))) = {0};
page_t mem_heapBasePT[512*3] __attribute__((aligned(PAGE_SIZE))) = {0};

/* Helper macro */
#define KERNEL_PHYS(x) (uintptr_t)((uintptr_t)(x) - 0xFFFFF00000000000)

// Log method
#define LOG(status, ...) dprintf_module(status, "ARCH:MEM", __VA_ARGS__);

/**
 * @brief Get the current directory (just current CPU)
 */
page_t *mem_getCurrentDirectory() {
    return current_cpu->current_dir;
}

/**
 * @brief Get the kernel page directory/root-level PML
 * @note RETURNS A VIRTUAL ADDRESS
 */
page_t *mem_getKernelDirectory() {
    return (page_t*)&mem_kernelPML[0];
}

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
 * @brief Switch the memory management directory
 * @param pagedir The virtual address of the page directory to switch to, or NULL for the kernel region
 * 
 * @warning Pass something mapped by mem_clone() or something in the identity-mapped PMM region.
 *          Anything greater than IDENTITY_MAP_MAXSIZE will be truncated in the PDBR.
 * 
 * @returns -EINVAL on invalid, 0 on success.
 */
int mem_switchDirectory(page_t *pagedir) {
    if (!pagedir) pagedir = mem_getKernelDirectory();
    if (current_cpu->current_dir == pagedir) return 0; // No need to waste time

    // !!!: Weird check, I'm really tired and want to fix this.
    // !!!: If something wants to load a pagedir from the physmem map, because it's 2MB paging,
    // !!!: mem_getPage will fail (and subsequently mem_getPhysicalAddress). Assume if mem_getPhysicalAddress
    // !!!: fails its a physical address (i hate my life)
    uintptr_t phys = mem_getPhysicalAddress(NULL, (uintptr_t)pagedir);
    if (phys == 0x0) {
        // Not correct, assume pagedir is a phys address or is physmem
        phys = (uintptr_t)pagedir & ~(MEM_PHYSMEM_MAP_REGION);
    }

    // Load PDBR
    asm volatile ("movq %0, %%cr3" :: "r"(phys & ~0xFFF));

    // Set current directory
    // !!!: THIS WILL CAUSE PROBLEMS IF THIS IS PHYSMEM MAPPED
    current_cpu->current_dir = pagedir;

    return 0;

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
 * @warning IMPORTANT: DO NOT FREE ANY PAGES. Just free the associated PML/PDPT/PD/PT.  
 * 
 * @warning Make sure the VAS being freed isn't the current one selected
 */
void mem_destroyVAS(page_t *vas) {
    // !!!: <256 - requires problem zone to be fixed in mem_clone()
    // !!!: Relies on VAS but doesn't do the *obvious* strategy of just using the VAS...
    for (size_t pml4e = 0; pml4e < 256; pml4e++) {
        if (vas[pml4e].bits.present) {
            // Get the PDPT
            page_t *pdpt = (page_t*)mem_remapPhys((vas[pml4e].bits.address << MEM_PAGE_SHIFT), 0);
            for (size_t pdpte = 0; pdpte < 512; pdpte++) {
                if (pdpt[pdpte].bits.present) {
                    // Get the PD
                    page_t *pd = (page_t*)mem_remapPhys((pdpt[pdpte].bits.address << MEM_PAGE_SHIFT), 0);
                    for (size_t pde = 0; pde < 512; pde++) {
                        if (pd[pde].bits.present) {
                            // Get the PT
                            page_t *pt = (page_t*)mem_remapPhys((pd[pde].bits.address << MEM_PAGE_SHIFT), 0);
                            
                            // !!!: ONLY FOR DEBUG
                            for (size_t pte = 0; pte < 512; pte++) {
                                page_t *pg = &pt[pte];
                                if (pg->bits.usermode && pg->bits.present && pg->bits.address && pg->bits.rw) {
                                    // Debug
                                    uintptr_t address = ((pml4e << (9 * 3 + 12)) | (pdpte << (9*2 + 12)) | (pde << (9 + 12)) | (pte << MEM_PAGE_SHIFT));
                                    // LOG(WARN, "Unfreed usermode page at address %016llX (frame: %p, cow waiting: %d, rw: %d) - FREE\n", address, MEM_GET_FRAME(pg), pg->bits.cow, pg->bits.rw);
                                }
                            }

                            // Free the PT
                            pd[pde].bits.present = 0;
                            pmm_freeBlock((pd[pde].bits.address << MEM_PAGE_SHIFT));
                        }
                    }

                    // Free the PD
                    pdpt[pdpte].bits.present = 0;
                    pmm_freeBlock((pdpt[pdpte].bits.address << MEM_PAGE_SHIFT));
                }
            }

            // Free the PDPT
            vas[pml4e].bits.present = 0;
            pmm_freeBlock((vas[pml4e].bits.address << MEM_PAGE_SHIFT));
        }
    }

    // Free the VAS
    pmm_freeBlock((uintptr_t)vas & ~MEM_PHYSMEM_MAP_REGION);
}

/**
 * @brief Copy a usermode page. 
 * @param src_page The source page
 * @param dest_page The destination page
 */
static void mem_copyUserPage(page_t *src_page, page_t *dest_page) {
    // Just copy the page
    uintptr_t src_frame = mem_remapPhys(MEM_GET_FRAME(src_page), PAGE_SIZE);
    uintptr_t dest_frame_block = pmm_allocateBlock();
    uintptr_t dest_frame = mem_remapPhys(dest_frame_block, PAGE_SIZE);
    memcpy((void*)dest_frame, (void*)src_frame, PAGE_SIZE);

    // Setup bits
    dest_page->data = src_page->data;
    MEM_SET_FRAME(dest_page, dest_frame_block);
    dest_page->bits.cow = 0; // Not CoW
    dest_page->bits.rw = 1;

    mem_unmapPhys(dest_frame, PAGE_SIZE);
    mem_unmapPhys(src_frame, PAGE_SIZE);
}


/**
 * @brief Clone a page directory.
 * 
 * This is a full PROPER page directory clone.
 * This function does it properly and clones the page directory, its tables, and their respective entries fully.
 * YOU SHOULD NOT DO COW ON THIS. The virtual address system (VAS) will handle CoW for you. 
 * 
 * @param dir The source page directory. Keep as NULL to clone the current page directory.
 * @returns The page directory on success
 */
page_t *mem_clone(page_t *dir) {
    if (!dir) dir = current_cpu->current_dir;

    // Create a new VAS
    page_t *dest = mem_createVAS();

    LOG(DEBUG, "[CLONE   ] Clone page directory %016llX -> %016llX\n", dir, dest);

    // Copy top half. This contains the kernel's important regions, including the heap
    // !!!: THIS IS A PROBLEM ZONE. The heap contains PDPTs/PDs/PTs that are premapped but not enough! With an infinitely
    // !!!: expanding heap we create issues where once we run out of a PDPT the heap won't update in other PMLs. Page fault
    // !!!: handlers can take care of this and remap PDPTs but for things like kernel stacks it might be better to map them as global
    // !!!: (avoiding flushing them in TLB)
    memcpy(&dest[256], &dir[256], 256 * sizeof(page_t));

    // Copy low PDPTs (i.e. usermode code location and kernel code)
    for (size_t pdpt = 0; pdpt < 256; pdpt++) {
        if (!(dir[pdpt].bits.present)) continue;
        page_t *pdpt_srcentry = &dir[pdpt]; // terrible naming lol

        page_t *pdpt_destentry = &dest[pdpt];

        // Create a new PDPT
        uintptr_t pdpt_dest_block = pmm_allocateBlock();
        page_t *pdpt_dest = (page_t*)mem_remapPhys(pdpt_dest_block, PAGE_SIZE);
        memset((void*)pdpt_dest, 0, PAGE_SIZE); 

        // Do a raw copy but set the frame
        pdpt_destentry->data = pdpt_srcentry->data;
        MEM_SET_FRAME(pdpt_destentry, pdpt_dest_block);

        // Now map in the existing PDPT
        page_t *pdpt_src = (page_t*)mem_remapPhys(MEM_GET_FRAME(pdpt_srcentry), PAGE_SIZE);

        // Copy PDs
        for (size_t pd = 0; pd < 512; pd++) {
            page_t *pd_srcentry = &pdpt_src[pd];
            if (!(pd_srcentry->bits.present)) continue;

            page_t *pd_destentry = &pdpt_dest[pd];

            // Allocate a new page directory
            uintptr_t pd_dest_block = pmm_allocateBlock();
            page_t *pd_dest  = (page_t*)mem_remapPhys(pd_dest_block, PAGE_SIZE);
            memset((void*)pd_dest, 0, PAGE_SIZE); 
        

            // Do a raw copy but set the frame
            pd_destentry->data = pd_srcentry->data;
            MEM_SET_FRAME(pd_destentry, pd_dest_block);

            // Now map in the existing PD
            page_t *pd_src = (page_t*)mem_remapPhys(MEM_GET_FRAME(pd_srcentry), PAGE_SIZE);

            // Copy PTs
            for (size_t pt = 0; pt < 512; pt++) {
                page_t *pt_srcentry = &pd_src[pt];
                if (!(pt_srcentry->bits.present)) continue;

                page_t *pt_destentry = &pd_dest[pt];

                // Allocate a new page table
                uintptr_t pt_dest_block = pmm_allocateBlock();
                page_t *pt_dest  = (page_t*)mem_remapPhys(pt_dest_block, PAGE_SIZE);
                memset((void*)pt_dest, 0, PAGE_SIZE);

                // Do a raw copy but set the frame
                pt_destentry->data = pt_srcentry->data;
                MEM_SET_FRAME(pt_destentry, pt_dest_block);

                // Now map in the existing PT
                page_t *pt_src = (page_t*)mem_remapPhys(MEM_GET_FRAME(pt_srcentry), PAGE_SIZE);

                // Copy pages
                for (size_t page = 0; page < 512; page++) {
                    page_t *page_src = &pt_src[page];
                    page_t *page_dest = &pt_dest[page];
                    if (!page_src || !(page_src->bits.present)) continue; // Not present

                    if (page_src->bits.usermode) {
                        uintptr_t address = ((pdpt << (9 * 3 + 12)) | (pd << (9*2 + 12)) | (pt << (9 + 12)) | (page << MEM_PAGE_SHIFT));
                        mem_copyUserPage(page_src, page_dest);
                        LOG(DEBUG, "Usermode page at address %016llX (frame: %p) - copy\n", address, MEM_GET_FRAME(page_src));
                    } else {
                        // Raw copy
                        page_dest->data = page_src->data;
                    }
                } 
            }
        }
    }
    return dest;
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
    if (!MEM_IS_CANONICAL(virt)) return;

    page_t *pg = mem_getPage(dir, virt, MEM_CREATE);
    if (pg) {
        mem_allocatePage(pg, MEM_PAGE_NOALLOC | flags);
        MEM_SET_FRAME(pg, phys);
    }
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
    // Validate that the address is canonical
    if (!MEM_IS_CANONICAL(address)) return NULL;

    // Align the address and get the dircetory
    uintptr_t addr = (address % PAGE_SIZE != 0) ? MEM_ALIGN_PAGE_DESTRUCTIVE(address) : address;
    page_t *directory = (dir == NULL) ? current_cpu->current_dir : dir;

    // Get the PML4
    page_t *pml4_entry = &(directory[MEM_PML4_INDEX(addr)]);
    if (!pml4_entry->bits.present) {
        if (!flags & MEM_CREATE) goto bad_page;

        // Allocate a new PML4 entry and zero it
        uintptr_t block = pmm_allocateBlock();
        uintptr_t block_remap = mem_remapPhys(block, PMM_BLOCK_SIZE);
        memset((void*)block_remap, 0, PMM_BLOCK_SIZE);

        // Setup the bits in the directory index
        pml4_entry->bits.present = 1;
        pml4_entry->bits.rw = 1;
        pml4_entry->bits.usermode = 1; // NOTE: As long as the entries here aren't usermode this should be fine... right?
        MEM_SET_FRAME(pml4_entry, block);

        mem_unmapPhys(block_remap, PMM_BLOCK_SIZE); // we don't even have to do this
    }

    // Get the PDPT and the entry
    page_t *pdpt = (page_t*)mem_remapPhys(MEM_GET_FRAME(pml4_entry), PMM_BLOCK_SIZE);
    page_t *pdpt_entry = &(pdpt[MEM_PDPT_INDEX(addr)]);
    
    if (!pdpt_entry->bits.present) {
        if (!flags & MEM_CREATE) goto bad_page;

        // Allocate a new PDPT entry and zero it
        uintptr_t block = pmm_allocateBlock();
        uintptr_t block_remap = mem_remapPhys(block, PMM_BLOCK_SIZE);
        memset((void*)block_remap, 0, PMM_BLOCK_SIZE);

        // Setup the bits in the directory index
        pdpt_entry->bits.present = 1;
        pdpt_entry->bits.rw = 1;
        pdpt_entry->bits.usermode = 1; // NOTE: As long as the entries here aren't usermode this should be fine... right?
        MEM_SET_FRAME(pdpt_entry, block);

        mem_unmapPhys(block_remap, PMM_BLOCK_SIZE); // we don't even have to do this
    }

    if (pdpt_entry->bits.size) {
        // LOG(WARN, "Tried to get page from a PDPT that is 1GiB\n");
        return NULL;
    }

    // Get the PD and the entry
    page_t *pd = (page_t*)mem_remapPhys(MEM_GET_FRAME(pdpt_entry), PMM_BLOCK_SIZE);
    page_t *pde = &(pd[MEM_PAGEDIR_INDEX(addr)]);

    if (!pde->bits.present) {
        if (!flags & MEM_CREATE) goto bad_page;

        // Allocate a new PDE and zero it
        uintptr_t block = pmm_allocateBlock();
        uintptr_t block_remap = mem_remapPhys(block, PMM_BLOCK_SIZE);

        memset((void*)block_remap, 0, PMM_BLOCK_SIZE);

        // Setup the bits in the directory index
        pde->bits.present = 1;
        pde->bits.rw = 1;
        pde->bits.usermode = 1; // NOTE: As long as the entries here aren't usermode this should be fine... right?
        MEM_SET_FRAME(pde, block);

        mem_unmapPhys(block_remap, PMM_BLOCK_SIZE); // we don't even have to do this
    }

    if (pde->bits.size) {
        // LOG(WARN, "Tried to get page from a PD that is 2MiB\n");
        return NULL;
    }

    // Get the table
    page_t *table = (page_t*)mem_remapPhys(MEM_GET_FRAME(pde), PMM_BLOCK_SIZE);
    page_t *pte = &(table[MEM_PAGETBL_INDEX(addr)]);

    // Return
    return pte;

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
    if (!page) return;

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

    if (flags & MEM_PAGE_WRITE_COMBINE) {
        page->bits.size = 1;
    }
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
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 * 
 * @note You need to call @c mem_unmapPhys when you are finished with the address.
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) {
    if (size > MEM_PHYSMEM_MAP_SIZE) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "mem", "*** Remapping physical address %016llX for %016llX, ran out of space.\n", frame_address, size);
        __builtin_unreachable();
    }

    return frame_address | MEM_PHYSMEM_MAP_REGION;
}

/**
 * @brief Unmap a PMM address in the identity mapped region
 * @param frame_address The address of the frame to unmap, as returned by @c mem_remapPhys
 * @param size The size of the frame to unmap
 */
void mem_unmapPhys(uintptr_t frame_address, uintptr_t size) {
    // No caching system is in place, no unmapping.
}


/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr) {
    if (!MEM_IS_CANONICAL(virtaddr)) return 0x0;

    uintptr_t offset = 0x000; // Offset in case address isn't page aligned
    if (virtaddr & 0xFFF) {
        offset = virtaddr & 0xFFF;
        virtaddr = virtaddr & ~0xFFF;
    } 
    
    page_t *pg = mem_getPage(dir, virtaddr, MEM_DEFAULT);

    if (pg) return MEM_GET_FRAME(pg) + offset;
    return 0x0;
}

/**
 * @brief Page fault handler
 * @param exception_index 14
 * @param regs Registers
 * @param extended Extended registers
 */
int mem_pageFault(uintptr_t exception_index, registers_t *regs, extended_registers_t *regs_extended) {
    // Check if this was a usermode page fault
    if (regs->cs != 0x08) {
        // Was this an exception because we didn't map their heap?
        if (regs_extended->cr2 >= current_cpu->current_process->heap_base && regs_extended->cr2 < current_cpu->current_process->heap) {
            // Yes, it was, handle appropriately by mapping this page
            mem_allocatePage(mem_getPage(NULL, regs_extended->cr2, MEM_CREATE), MEM_DEFAULT);
            return 0;
        }

        if (regs_extended->cr2 > MEM_USERMODE_STACK_REGION && regs_extended->cr2 < MEM_USERMODE_STACK_REGION + PAGE_SIZE*2) {
            mem_allocatePage(mem_getPage(NULL, regs_extended->cr2, MEM_CREATE), MEM_DEFAULT);
            return 0;
        }

        // Check for VAS fault
        // Default hint is 0x2000
        if (vas_fault(current_cpu->current_process->vas, regs_extended->cr2, 0x2000)) {
            return 0;
        }

        // TODO: This code can probably bug out - to be extensively tested
        printf(COLOR_CODE_RED "Process \"%s\" (PID: %d) encountered a page fault at address %p and will be shutdown\n" COLOR_CODE_RESET, current_cpu->current_process->name, current_cpu->current_process->pid, regs_extended->cr2);
        
        // Dump debug information
        LOG(ERR, "Process \"%s\" (PID: %d) encountered page fault at %p with no valid resolution (error code: 0x%x). Shutdown\n", current_cpu->current_process->name, current_cpu->current_process->pid, regs_extended->cr2, regs->err_code);
        LOG(ERR, "The fault occurred @ IP %04x:%016llX SP %016llX\n", regs->cs, regs->rip, regs->rsp);
        vas_dump(current_cpu->current_process->vas);
        
        // Perform traceback
        LOG(ERR, "STACK BACKTRACE:\n");
        LOG(ERR, "Starting @ IP: %016llX\n", regs->rip);
        stack_frame_t *stk = (stack_frame_t*)regs->rbp;
        while (stk) {
            if (!mem_validate(stk, PTR_USER)) {
                LOG(ERR, "Corrupted stack frame 0x%016llX detected\n", stk);
                break;
            }

            LOG(ERR, "FRAME 0x%016llX: 0x%016llX\n", stk, stk->ip);
        
            stk = stk->nextframe;
        }


        process_exit(current_cpu->current_process, 1);
        return 0;
    }

    if (current_cpu->current_process && current_cpu->current_process->vas) {
        // Check for fault
        if (vas_fault(current_cpu->current_process->vas, regs_extended->cr2, 0x2000)) {
            return 0;
        }
    }

    uintptr_t page_fault_addr = 0x0;
    asm volatile ("movq %%cr2, %0" : "=a"(page_fault_addr));

    LOG(ERR, "#PF (%016llX): IP %04x:%016llX SP %016llX\n", page_fault_addr, regs->cs, regs->rip, regs->rsp);

    // Page fault, get the address
    kernel_panic_prepare(CPU_EXCEPTION_UNHANDLED);
    
    // Print it out
    LOG(NOHEADER, "*** ISR detected exception: Page fault at address 0x%016llX\n\n", page_fault_addr);
    printf("*** Page fault at address 0x%016llX detected in kernel.\n", page_fault_addr);

    LOG(NOHEADER, "\033[1;31mFAULT REGISTERS:\n\033[0;31m");

    LOG(NOHEADER, "RAX %016llX RBX %016llX RCX %016llX RDX %016llX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    LOG(NOHEADER, "RDI %016llX RSI %016llX RBP %016llX RSP %016llX\n", regs->rdi, regs->rsi, regs->rbp, regs->rsp);
    LOG(NOHEADER, "R8  %016llX R9  %016llX R10 %016llX R11 %016llX\n", regs->r8, regs->r9, regs->r10, regs->r11);
    LOG(NOHEADER, "R12 %016llX R13 %016llX R14 %016llX R15 %016llX\n", regs->r12, regs->r13, regs->r14, regs->r15);
    LOG(NOHEADER, "ERR %016llX RIP %016llX RFL %016llX\n\n", regs->err_code, regs->rip, regs->rflags);

    LOG(NOHEADER, "CS %04X DS %04X SS %04X\n\n", regs->cs, regs->ds, regs->ss);
    LOG(NOHEADER, "CR0 %08X CR2 %016llX CR3 %016llX CR4 %08X\n", regs_extended->cr0, regs_extended->cr2, regs_extended->cr3, regs_extended->cr4);
    LOG(NOHEADER, "GDTR %016llX %04X\n", regs_extended->gdtr.base, regs_extended->gdtr.limit);
    LOG(NOHEADER, "IDTR %016llX %04X\n", regs_extended->idtr.base, regs_extended->idtr.limit);

    // !!!: not conforming (should call kernel_panic_finalize) but whatever
    // We want to do our own traceback.
extern void arch_panic_traceback(int depth, registers_t *regs);
    arch_panic_traceback(10, regs);

    // Show core processes
    LOG(NOHEADER, COLOR_CODE_RED_BOLD "\nCPU DATA:\n" COLOR_CODE_RED);

    for (int i = 0; i < MAX_CPUS; i++) {
        if (processor_data[i].cpu_id || !i) {
            // We have valid data here
            if (processor_data[i].current_thread != NULL) {
                LOG(NOHEADER, COLOR_CODE_RED "CPU%d: Current thread %p (process '%s') - page directory %p\n", i, processor_data[i].current_thread, processor_data[i].current_process->name, processor_data[i].current_dir);
            } else {
                LOG(NOHEADER, COLOR_CODE_RED "CPU%d: No thread available. Page directory %p\n", processor_data[i].current_dir);
            }
        }
    }

    // Display message
    LOG(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);
}

/**
 * @brief Initialize the memory management subsystem
 * 
 * This function will identity map the kernel into memory and setup page tables.
 * For x86_64, it also sets up the PMM allocator.
 * 
 * @warning MEM_HEAP_REGION is hardcoded into this - we can probably use mem_mapAddress??
 * @warning This is some pretty messy code, I'm sorry :(
 * 
 * @param mem_size The size of memory (aka highest possible address)
 * @param first_free_page The first free page after the kernel
 */
void mem_init(uintptr_t mem_size, uintptr_t first_free_page) {
    // Set the initial page region as the current page for this core.
    current_cpu->current_dir = (page_t*)&mem_kernelPML;

    LOG(INFO, "Initializing memory system - memory size is %016llX, first free page is %016llX\n", mem_size, first_free_page);

    // Get kernel address
extern uintptr_t __kernel_start, __kernel_end;
    uintptr_t kernel_addr = MEM_ALIGN_PAGE((uintptr_t)&__kernel_end);

    // First, we need to remap the kernel
    // Calculate how many pages the kernel needs
    size_t kernel_pages = (MEM_ALIGN_PAGE(kernel_addr - (uintptr_t)&__kernel_start) / PAGE_SIZE);
    size_t kernel_pts = (kernel_pages >= 512) ? (kernel_pages / 512) + ((kernel_pages%512) ? 1 : 0) : 1;
    kernel_pts++; // !!!: Accounts for what is probably bad rounding on my part..

    // Sanity check to make sure Hexahedron isn't bloated
    if ((kernel_pts / 512) / 512 > 1) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Hexahedron is too big - requires %i PDPTs when 1 is given\n", (kernel_pts / 512) / 512);
        __builtin_unreachable();
    }

    // I'm lazy :)
    if (kernel_pts / 512 > 1) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Hexahedron is too big - multiple low base PDs have not been implemented (requires %i PDs)\n", kernel_pts / 512);
        __builtin_unreachable();
    }
    
    // This one I'll probably have to come back and fix
    if (kernel_pts > 12) {
        kernel_panic_extended(MEMORY_MANAGEMENT_ERROR, "mem", "*** Hexahedron is too big - >3 low base PTs have not been implemented (requires %i PTs)\n", kernel_pts);
        __builtin_unreachable();
    }

    LOG(DEBUG, "Recreating kernel map (kernel is using %d KB in memory)...\n", (kernel_pages * PAGE_SIZE) / 1024);

    // Make the kernel map
    mem_highBasePDPT[0].bits.address = (KERNEL_PHYS(&mem_highBasePD)) >> MEM_PAGE_SHIFT;
    mem_highBasePDPT[0].bits.present = 1;
    mem_highBasePDPT[0].bits.rw = 1;
    
    for (size_t i = 0; i < kernel_pts; i++) {
        mem_highBasePD[i].bits.address = KERNEL_PHYS(&mem_highBasePTs[i*512]) >> MEM_PAGE_SHIFT;
        mem_highBasePD[i].bits.present = 1;
        mem_highBasePD[i].bits.rw = 1;

        for (size_t j = 0; j < 512; j++) {
            mem_highBasePTs[(i * 512) + j].bits.address = ((PAGE_SIZE*512) * i + PAGE_SIZE * j) >> MEM_PAGE_SHIFT;
            mem_highBasePTs[(i * 512) + j].bits.present = 1;
            mem_highBasePTs[(i * 512) + j].bits.rw = 1;
        }
    }

    // Set it now
    mem_kernelPML[0][MEM_PML4_INDEX(((uintptr_t)&__kernel_start))].bits.address = KERNEL_PHYS(&mem_highBasePDPT) >> MEM_PAGE_SHIFT;

    // Build the identity map
    int identity_map_idx = MEM_PML4_INDEX(MEM_PHYSMEM_MAP_REGION);
    LOG(DEBUG, "Initializing physical memory mapping (%p - PML %d)...\n", MEM_PHYSMEM_MAP_REGION, identity_map_idx);

    // Identity map from -128GB using 2MiB pages
    // !!!: == THIS IS REALLY BAD (but makes things quick)
    // !!!: We are basically going to use 2MiB pages in the identity map region and not use caching, since it isn't
    // !!!: required. This is bad because most things expect a 4KB page, but 2MiB pages mean that we can fit a lot more.
    // !!!: See https://github.com/klange/toaruos/blob/a54a0cbbee1ac18bceb2371e49876eab9abb3a11/kernel/arch/x86_64/mmu.c#L1048
    
    // !!!: Not only is this using 2MB paging bad, it also maps unnecessary memory.
    for (size_t i = 0; i < MEM_PHYSMEM_MAP_SIZE / PAGE_SIZE_LARGE / 512; i++) {
        mem_identityBasePDPT[i].bits.address = KERNEL_PHYS(&mem_identityBasePD[i]) >> MEM_PAGE_SHIFT;
        mem_identityBasePDPT[i].bits.present = 1;
        mem_identityBasePDPT[i].bits.rw = 1;
        mem_identityBasePDPT[i].bits.usermode = 1;  // NOTE: Do we want to do this?

        // Using 512-page blocks
        for (size_t j = 0; j < 512; j++) {
            mem_identityBasePD[i][j].data = ((i << 30) + (j << 21)) | 0x80 | 0x03;
        }
    }

    // Set it in the kernel
    mem_kernelPML[0][MEM_PML4_INDEX(MEM_PHYSMEM_MAP_REGION)].data = KERNEL_PHYS(&mem_identityBasePDPT) | 0x7;

    // Now we need to map the heap
    LOG(DEBUG, "Initializing kernel heap mapping (%p  - PML %d)...\n", MEM_HEAP_REGION, MEM_PML4_INDEX(MEM_HEAP_REGION));

    // Calculate the amount of pages required for our PMM
    size_t frame_bytes = PMM_INDEX_BIT((mem_size >> 12) * 8);
    frame_bytes = MEM_ALIGN_PAGE(frame_bytes);
    size_t frame_pages = frame_bytes >> MEM_PAGE_SHIFT;

    if (frame_pages > 512 * 3) {
        // 512 * 3 = size of the heap base provided, so that's not great.
        LOG(WARN, "Too much memory available - %i pages required for allocation bitmap (max 1536)\n", frame_pages);
        // TODO: We can probably resolve this by just rewriting some parts of mem_mapAddress
    }

    // Setup hierarchy (ugly)
    mem_heapBasePDPT[0].bits.address = (KERNEL_PHYS(&mem_heapBasePD) >> MEM_PAGE_SHIFT);
    mem_heapBasePDPT[0].bits.present = 1;
    mem_heapBasePDPT[0].bits.rw = 1;
    mem_heapBasePDPT[0].bits.usermode = 1;

    // Create PD mappings
    for (int i = 0; i < 3; i++) {
        mem_heapBasePD[i].bits.address = (KERNEL_PHYS(&mem_heapBasePT[i*512])) >> MEM_PAGE_SHIFT;
        mem_heapBasePD[i].bits.present = 1;
        mem_heapBasePD[i].bits.rw = 1;
        mem_heapBasePD[i].bits.usermode = 1;
    }

    // Map a bunch o' entries
    for (size_t i = 0; i < frame_pages; i++) {
        mem_heapBasePT[i].bits.address = ((first_free_page + (i << 12)) >> MEM_PAGE_SHIFT);
        mem_heapBasePT[i].bits.present = 1;
        mem_heapBasePT[i].bits.rw = 1;
    }

    // Set it in PML
    mem_kernelPML[0][MEM_PML4_INDEX(MEM_HEAP_REGION)].bits.address = ((KERNEL_PHYS(&mem_heapBasePDPT)) >> MEM_PAGE_SHIFT);
    mem_kernelPML[0][MEM_PML4_INDEX(MEM_HEAP_REGION)].bits.present = 1;
    mem_kernelPML[0][MEM_PML4_INDEX(MEM_HEAP_REGION)].bits.rw = 1;
    mem_kernelPML[0][MEM_PML4_INDEX(MEM_HEAP_REGION)].bits.usermode = 1;


    // Tada. We've finished setting up our heap, so now we need to use mem_remapPhys to remap our PML.
    current_cpu->current_dir = (page_t*)mem_remapPhys((uintptr_t)current_cpu->current_dir, 0x0); // Size is arbitrary

    // Now that we have a heap mapped, we can allocate frame bytes.
    uintptr_t *frames = (uintptr_t*)MEM_HEAP_REGION;

    // Initialize PMM
    LOG(DEBUG, "Initializing physical memory manager...\n")
    pmm_init(mem_size, frames); 

    // Call back to architecture to mark/unmark memory
    // !!!: Unmarking too much memory. Would kernel_addr work?
    extern void arch_mark_memory(uintptr_t highest_address, uintptr_t mem_size);
    arch_mark_memory(MEM_ALIGN_PAGE(first_free_page + frame_bytes), mem_size);

    // Setup kernel heap to point to after frames
    mem_kernelHeap = MEM_HEAP_REGION + frame_bytes + PAGE_SIZE;

    LOG(DEBUG, "Kernel heap will begin at %p\n", mem_kernelHeap);

    LOG(DEBUG, "Removing original kernel allocation...\n");
    mem_kernelPML[0][0].bits.present = 0;

    // Now that we have finished creating our basic memory system, we can map the kernel code to be R/O.
    // Force map kernel code (text section)
    extern uintptr_t __text_start, __text_end;
    uintptr_t kernel_code_start = (uintptr_t)&__text_start;
    uintptr_t kernel_code_end = (uintptr_t)&__text_end;

    // Align kernel code end (as text section is positioned at the beginning)
    kernel_code_end = kernel_code_end & ~0xFFF; // This should gurantee that we don't overmap into, say, BSS

    for (uintptr_t i = kernel_code_start; i < kernel_code_end; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
        if (pg) pg->bits.rw = 0;
    }

    // Setup the PAT
    // TODO: Write a better interface for the PAT
    asm volatile(
    	"movl $0x277, %%ecx\n"
    	"rdmsr\n"
    	"movw $0x0401, %%dx\n"
    	"wrmsr\n"
    	::: "eax", "ecx", "edx", "memory"
    );

    // Enable WP
    asm volatile (
        "movq %%cr0, %%rax\n"
        "orq $0x10000, %%rax\n"
        "movq %%rax, %%cr0" ::: "rax");



    // Initialize regions
    mem_regionsInitialize();

    // Register page fault
    hal_registerExceptionHandler(14, mem_pageFault);

    LOG(INFO, "Memory management initialized\n");
}

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b) { // Sanity checks
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
 * @note This is a newer addition to the memory subsystem. It may seem like it doesn't fit in.
 */
uintptr_t mem_allocate(uintptr_t start, size_t size, uintptr_t flags, uintptr_t page_flags) {
    if (!size) return start;

    // Save variables
    uintptr_t start_original = start;
    size_t size_original = size;

    // Check
    if (!MEM_IS_CANONICAL(start)) goto _error;

    // Sanity checks
    if (start == 0x0 && !(flags & MEM_ALLOC_HEAP)) {
        LOG(WARN, "Cannot allocate to 0x0 (MEM_ALLOC_HEAP not specified)\n");
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
                LOG(ERR, "Fragile allocation failed - found present page at %p\n", i);
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
            LOG(ERR, "Could not get page at %p\n", i);
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
    if (!MEM_IS_CANONICAL(start)) return; // Corruption?
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
            LOG(WARN, "Tried to free page %p but it is not present (?)\n", i);
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