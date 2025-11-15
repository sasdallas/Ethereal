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
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/misc/spinlock.h>
#include <kernel/task/process.h>
#include <kernel/misc/util.h>

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
    assert(0);
    return NULL;
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
inline void mem_invalidatePage(uintptr_t addr) {
    STUB();
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
    STUB();
}

/**
 * @brief Create a new, completely blank virtual address space
 * @returns A pointer to the VAS
 */
page_t *mem_createVAS() {
    STUB();
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
    STUB();
}

/**
 * @brief Copy a usermode page. 
 * @param src_page The source page
 * @param dest_page The destination page
 */
static void mem_copyUserPage(page_t *src_page, page_t *dest_page) {
    STUB();
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
    STUB();
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
    STUB();
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
    STUB();
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
    STUB();
}

/**
 * @brief Free a page
 * 
 * @param page The page to free
 */
void mem_freePage(page_t *page) {
    STUB();
}


/**
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 * 
 * @note You need to call @c mem_unmapPhys when you are finished with the address.
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) {
    STUB();
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
    STUB();
}

/**
 * @brief Page fault handler
 * @param exception_index 14
 * @param regs Registers
 * @param extended Extended registers
 */
int mem_pageFault(uintptr_t exception_index, registers_t *regs, extended_registers_t *regs_extended) {
    STUB();
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
    STUB();
}

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b) {
    STUB();
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
    STUB();
}

/**
 * @brief Free a region of memory
 * @param start The starting virtual address (must be specified)
 * @param size How much memory was allocated (will be aligned)
 * @param flags Flags to use for @c mem_free (e.g. MEM_ALLOC_HEAP)
 * @note Most flags do not affect @c mem_free
 */
void mem_free(uintptr_t start, size_t size, uintptr_t flags) {
    STUB()
}

/**
 * @brief Validate a specific pointer in memory
 * @param ptr The pointer you wish to validate
 * @param flags The flags the pointer must meet - by default, kernel mode and R/W (see PTR_...)
 * @returns 1 on a valid pointer
 */
int mem_validate(void *ptr, unsigned int flags) {
    STUB();
}