/**
 * @file hexahedron/include/kernel/mem/mem.h
 * @brief Memory system functions
 * 
 * This file is an interface for the memory mapper
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__ARCH_I386__) || defined(__INTELLISENSE__)
#include <kernel/arch/i386/mem.h> // Arch-specific definitions, like directory, entries, etc)
#elif defined(__ARCH_X86_64__) || defined(__INTELLISENSE__)
#include <kernel/arch/x86_64/mem.h>
#else
#error "Unsupported architecture for memory management"
#endif

#include <kernel/mem/regions.h>


/**** DEFINITIONS ****/

// General flags
#define MEM_DEFAULT             0x00    // Default settings to everything. For mem_allocatePage: usermode, writable, and present

// Flags to mem_getPage
#define MEM_CREATE              0x01    // Create the page. Commonly used with mem_getPage during mappings

// Flags to mem_allocatePage
#define MEM_PAGE_KERNEL             0x02    // The page is kernel-mode only
#define MEM_PAGE_READONLY           0x04    // The page is read-only
#define MEM_PAGE_WRITETHROUGH       0x08    // The page is marked as writethrough
#define MEM_PAGE_NOT_CACHEABLE      0x10    // The page is not cacheable
#define MEM_PAGE_NOT_PRESENT        0x20    // The page is not present in memory
#define MEM_PAGE_NOALLOC            0x40    // Do not allocate the page and instead use what was given
#define MEM_PAGE_FREE               0x80    // Free the page. Sets it to zero if specified in mem_allocatePage
#define MEM_PAGE_NO_EXECUTE         0x100   // (x86_64 only) Set the page as non-executable.
#define MEM_PAGE_WRITE_COMBINE      0x200   // Sets up the page as write-combining if the architecture supports it

// Flags to mem_allocate
#define MEM_ALLOC_CONTIGUOUS        0x01    // Allocate contiguous blocks of memory, rather than fragmenting PMM blocks
#define MEM_ALLOC_FRAGILE           0x02    // Fragile, meaning that a check is first run to determine if any pages are already marked as present
#define MEM_ALLOC_HEAP              0x04    // Allocate from the heap
#define MEM_ALLOC_CRITICAL          0x08    // This allocation is critical, on failure terminate kernel

// Flags to mem_validate
#define PTR_USER                    0x01    // Usermode or kernel pointer (STRICT: Only usermode pointer)
#define PTR_READONLY                0x02    // Read-only or read-write pointer (STRICT: Only read-only)
#define PTR_STRICT                  0x04    // Strict pointer validation

/**** FUNCTIONS ****/

/**
 * @brief Get the physical address of a virtual address
 * @param dir Can be NULL to use the current directory.
 * @param virtaddr The virtual address
 * 
 * @returns NULL on a PDE not being present or the address
 */
uintptr_t mem_getPhysicalAddress(page_t *dir, uintptr_t virtaddr);

/**
 * @brief Returns the page entry requested
 * @param dir The directory to search. Specify NULL for current directory
 * @param address The virtual address of the page (will be aligned for you if not aligned)
 * @param flags The flags of the page to look for
 * 
 * @warning Specifying MEM_CREATE will only create needed structures, it will NOT allocate the page!
 *          Please use a function such as mem_allocatePage to do that.
 */
page_t *mem_getPage(page_t *dir, uintptr_t address, uintptr_t flags);

/**
 * @brief Switch the memory management directory
 * @param pagedir The virtual address of the page directory to switch to, or NULL for the kernel region
 * 
 * @warning If bootstrapping, it is best to load this yourself. This method may rely on things like mem_getPhysicalAddress
 * 
 * @returns -EINVAL on invalid, 0 on success.
 */
int mem_switchDirectory(page_t *pagedir);

/**
 * @brief Get the kernel page directory/root-level PML
 * @note RETURNS A VIRTUAL ADDRESS
 */
page_t *mem_getKernelDirectory();

/**
 * @brief Map a physical address to a virtual address
 * 
 * @param dir The directory to map them in (leave blank for current)
 * @param phys The physical address
 * @param virt The virtual address
 * @param flags Additional flags to use (e.g. MEM_KERNEL)
 */
void mem_mapAddress(page_t *dir, uintptr_t phys, uintptr_t virt, int flags);

/**
 * @brief Allocate a page using the physical memory manager
 * 
 * @param page The page object to use. Can be obtained with mem_getPage
 * @param flags The flags to follow when setting up the page
 * 
 * @note You can also use this function to set bits of a specific page - just specify @c MEM_NOALLOC in @p flags.
 * @warning The function will automatically allocate a PMM block if NOALLOC isn't specified
 */
void mem_allocatePage(page_t *page, uintptr_t flags);

/**
 * @brief Remap a PMM address to the identity mapped region
 * @param frame_address The address of the frame to remap
 * @param size The size of the address to remap
 * 
 * @note You need to call @c mem_unmapPhys when you are finished with the address.
 */
uintptr_t mem_remapPhys(uintptr_t frame_address, uintptr_t size) ;

/**
 * @brief Unmap a PMM address in the identity mapped region
 * @param frame_address The address of the frame to unmap, as returned by @c mem_remapPhys
 * @param size The size of the frame to unmap
 */
void mem_unmapPhys(uintptr_t frame_address, uintptr_t size);

/**
 * @brief Get the current page directory/root-level PML
 */
page_t *mem_getCurrentDirectory();

/**
 * @brief Increment a page refcount
 * @param page The page to increment reference counts of
 * @returns The number of reference counts or 0 if maximum is reached
 */
int mem_incrementPageReference(page_t *page);

/**
 * @brief Decrement a page refcount
 * @param page The page to decrement the reference count of
 * @returns The number of reference counts. Panicks if 0
 */
int mem_decrementPageReference(page_t *page);

/**
 * @brief Create a new, completely blank virtual address space
 * @returns A pointer to the VAS
 */
page_t *mem_createVAS();

/**
 * @brief Destroys and frees the memory of a VAS
 * @param vas The VAS to destroy
 * 
 * @warning Make sure the VAS being freed isn't the current one selected
 */
void mem_destroyVAS(page_t *vas);

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
page_t *mem_clone(page_t *dir);

/**
 * @brief Free a page
 * 
 * @param page The page to free 
 */
void mem_freePage(page_t *page);

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
 * @brief Unmap an MMIO region
 * @param virt The virtual address returned by @c mem_mapMMIO
 * @param size The size requested by @c mem_mapMMIO
 */
void mem_unmapMMIO(uintptr_t virt, uintptr_t size);

/**
 * @brief Allocate a DMA region from the kernel
 * 
 * DMA regions are contiguous blocks that currently cannot be destroyed
 */
uintptr_t mem_allocateDMA(uintptr_t size);

/**
 * @brief Unallocate a DMA region from the kernel
 * 
 * @param base The address returned by @c mem_allocateDMA
 * @param size The size of the base
 */
void mem_freeDMA(uintptr_t base, uintptr_t size);

/**
 * @brief Map a driver into memory
 * @param size The size of the driver in memory
 * @returns A pointer to the mapped space
 */
uintptr_t mem_mapDriver(size_t size);

/**
 * @brief Unmap a driver from memory
 * @param base The base address of the driver in memory
 * @param size The size of the driver in memory
 */
void mem_unmapDriver(uintptr_t base, size_t size);

/**
 * @brief Expand/shrink the kernel heap
 * 
 * @param b The amount of bytes to allocate/free, needs to a multiple of PAGE_SIZE
 * @returns Address of the start of the bytes when allocating, or the previous address when shrinking
 */
uintptr_t mem_sbrk(int b);

/**
 * @brief Enable/disable paging
 * @param status Enable/disable
 */
void mem_setPaging(bool status);

/**
 * @brief Get the current position of the kernel heap
 * @returns The current position of the kernel heap
 */
uintptr_t mem_getKernelHeap();

/**
 * @brief Allocate a region of memory
 * @param start The starting virtual address (OPTIONAL IF YOU SPECIFY MEM_ALLOC_HEAP)
 * @param size How much memory to allocate (will be aligned)
 * @param flags Flags to use for @c mem_allocate (e.g. MEM_ALLOC_CONTIGUOUS)
 * @param page_flags Flags to use for @c mem_allocatePage (e.g. MEM_PAGE_KERNEL)
 * @returns Pointer to the new region of memory or 0x0 on failure
 */
uintptr_t mem_allocate(uintptr_t start, size_t size, uintptr_t flags, uintptr_t page_flags);

/**
 * @brief Free a region of memory
 * @param start The starting virtual address (must be specified)
 * @param size How much memory was allocated (will be aligned)
 * @param flags Flags to use for @c mem_free (e.g. MEM_ALLOC_HEAP)
 * @note Most flags do not affect @c mem_free
 */
void mem_free(uintptr_t start, size_t size, uintptr_t flags);

/**
 * @brief Validate a specific pointer in memory
 * @param ptr The pointer you wish to validate
 * @param flags The flags the pointer must meet - by default, kernel mode and R/W (see PTR_...)
 * @returns 1 on a valid pointer
 */
int mem_validate(void *ptr, unsigned int flags);

#endif