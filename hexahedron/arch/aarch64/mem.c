/**
 * @file hexahedron/arch/aarch64/mem.c
 * @brief aarch64 memory system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

// Standard includes
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

// Memory includes
#include <kernel/mem/mem.h>
#include <kernel/arch/aarch64/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/regions.h>

// General kernel includes
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/processor_data.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/pool.h>
#include <kernel/misc/spinlock.h>

// Heap/MMIO/driver space
uintptr_t mem_kernelHeap                = 0xAAAAAAAAAAAAAAAA;   // Kernel heap

// Stub variables (for debugger)
uintptr_t mem_mapPool                   = 0xAAAAAAAAAAAAAAAA;
uintptr_t mem_identityMapCacheSize      = 0xAAAAAAAAAAAAAAAA;   

/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return NULL;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Get the kernel page directory/root-level PML
 * @note RETURNS A VIRTUAL ADDRESS
 */
page_t *mem_getKernelDirectory() {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
}

/**
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 * 
 * @note You need to call @c mem_unmapPhys when you are finished with the address.
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Unmap a PMM address in the identity mapped region
 * @param frame_address The address of the frame to unmap, as returned by @c mem_remapPhys
 * @param size The size of the frame to unmap
 */
void mem_unmapPhys(uintptr_t frame_address, uintptr_t size) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
}

/**
 * @brief Get the current page directory/root-level PML
 */
page_t *mem_getCurrentDirectory() {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Create a new, completely blank virtual address space
 * @returns A pointer to the VAS
 */
page_t *mem_createVAS() {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Free a page
 * 
 * @param page The page to free 
 */
void mem_freePage(page_t *page) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
}

/**
 * @brief Create an MMIO region
 * @param phys The physical address of the MMIO space
 * @param size Size of the requested space (must be aligned)
 * @returns Address to new mapped MMIO region
 * 
 * @warning MMIO regions cannot be destroyed.
 */
uintptr_t mem_mapMMIO(uintptr_t phys, size_t size);

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Enable/disable paging
 * @param status Enable/disable
 */
void mem_setPaging(bool status) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
}

/**
 * @brief Get the current position of the kernel heap
 * @returns The current position of the kernel heap
 */
uintptr_t mem_getKernelHeap() {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}

/**
 * @brief Allocate a region of memory
 * @param start The starting virtual address (OPTIONAL IF YOU SPECIFY MEM_ALLOC_HEAP)
 * @param size How much memory to allocate (will be aligned)
 * @param flags Flags to use for @c mem_allocate (e.g. MEM_ALLOC_CONTIGUOUS)
 * @param page_flags Flags to use for @c mem_allocatePage (e.g. MEM_PAGE_KERNEL)
 * @returns Pointer to the new region of memory or 0x0 on failure
 */
uintptr_t mem_allocate(uintptr_t start, size_t size, uintptr_t flags, uintptr_t page_flags) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
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
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return;
}

/**
 * @brief Validate a specific pointer in memory
 * @param ptr The pointer you wish to validate
 * @param flags The flags the pointer must meet - by default, kernel mode and R/W (see PTR_...)
 * @returns 1 on a valid pointer
 */
int mem_validate(void *ptr, unsigned int flags) {
    kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "*** %s\n", __FUNCTION__);
    return 0x0;
}