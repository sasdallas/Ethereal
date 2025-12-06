/**
 * @file hexahedron/include/kernel/mem/vas.h
 * @brief Virtual address space manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_VAS_H
#define KERNEL_MEM_VAS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <kernel/mm/vmm.h>
#include <kernel/misc/spinlock.h>

/**** DEFINITIONS ****/

#define VAS_KERNEL          0x0     // The VAS is for kernel mode objects
#define VAS_COW             0x0     // Use copy on write for the VAS
#define VAS_FAKE            0x0     // Use fake memory for the VAS
#define VAS_NOT_GLOBAL      0x0     // The VAS is specific to the current CPU's directory

#define VAS_USERMODE        0x1     // The VAS is for usermode objects
#define VAS_NO_COW          0x2     // Do not use copy on write for this AVS
#define VAS_ONLY_REAL       0x4     // Do not give fake memory to the VAS
#define VAS_GLOBAL          0x8     // The VAS is global and should be replicated across all directories

/* Allocation protection flags */
#define VAS_PROT_READ       0x1     // Allow reads
#define VAS_PROT_WRITE      0x2     // Allow writes
#define VAS_PROT_EXEC       0x4     // Allow execution

#define VAS_PROT_DEFAULT    VAS_PROT_READ | VAS_PROT_WRITE | VAS_PROT_EXEC

/* Types of allocations (I just add one when I want to) */
#define VAS_ALLOC_NORMAL        1       // Normal allocation
#define VAS_ALLOC_MMAP          2       // Memory mapping that should remain untouched
#define VAS_ALLOC_MMAP_SHARE    3       // Shared allocation
#define VAS_ALLOC_THREAD_STACK  4       // Thread stack
#define VAS_ALLOC_EXECUTABLE    5       // Executable memory
#define VAS_ALLOC_PROG_BRK      6       // Program brk
#define VAS_ALLOC_SIGNAL_TRAMP  7       // Signal trampoline

/**** TYPES ****/

/**
 * @brief Virtual address space allocation
 * 
 * These allocations are given out by @c vas_allocate and placed in the VAS.
 * When new allocations are needed the VAS searches for holes in the system
 */
typedef struct vas_allocation {
    uintptr_t base;                 // Base of allocation
    size_t size;                    // Size of allocation
    uint8_t prot;                   // Protection flags
    uint8_t type;                   // Type of allocation

    spinlock_t ref_lck;             // Reference count lock
    uint8_t pending_cow;            // Pending CoW (also acquire the lock if you modify this)
    uint8_t references;             // References to this allocation
} vas_allocation_t;

/**
 * @brief Virtual address space node
 */
typedef struct vas_node {
    vas_allocation_t *alloc;        // Allocation
    struct vas_node *next;          // Next allocation in the chain
    struct vas_node *prev;          // Previous allocation in the chain
} vas_node_t;


/**
 * @brief Virtual address space
 */
typedef struct vas {
    char *name;             // Optional name for the VAS
    uintptr_t base;         // Base address of the VAS
    size_t size;            // Size of the VAS
    int flags;              // VAS flags
    spinlock_t *lock;       // VAS lock
    
    // NOTE: Not using list structures here despite the fact they're literally just this
    size_t allocations;     // Number of allocations
    vas_node_t *head;       // Start allocation
    vas_node_t *tail;       // End allocation
} vas_t;    

/**** FUNCTIONS ****/

/**
 * @brief Create a new virtual address space in memory
 * @param name The name of the VAS (optional)
 * @param address The base address of the VAS
 * @param size The initial size of the VAS
 * @param flags VAS creation flags
 */
vas_t *vas_create(char *name, uintptr_t address, size_t size, int flags);

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
vas_allocation_t *vas_reserve(vas_t *vas, uintptr_t address, size_t size, int type);

/**
 * @brief Allocate memory in a VAS
 * @param vas The VAS to reserve allocations in
 * @param size The amount of memory to allocate
 * @returns The allocation or NULL if no memory could be found
 */
vas_allocation_t *vas_allocate(vas_t *vas, size_t size);

/**
 * @brief Free memory in a VAS
 * @param vas The VAS to free the memory in
 * @param node The node to free
 * @param mem_freed Whether to actually free the memory, or whether it was freed/unmapped already
 * @returns 0 on success or 1 on failure
 */
int vas_free(vas_t *vas, vas_node_t *node, int mem_freed);

/**
 * @brief Get an allocation from a VAS
 * @param vas The VAS to get the allocation from
 * @param address The address to find the allocation for
 * @returns The node of the allocation or NULL if it could not be found
 */
vas_node_t *vas_get(vas_t *vas, uintptr_t address);

/**
 * @brief Dump VAS information
 */
void vas_dump(vas_t *vas);

/**
 * @brief Handle a VAS fault and give the memory if needed
 * @param vas The VAS
 * @param address The address the fault occurred at
 * @param size If the allocation spans over a page, this will be used as a hint for the amount to actually map in (for speed)
 * @returns 1 on fault resolution
 */
int vas_fault(vas_t *vas, uintptr_t address, size_t size);

/**
 * @brief Destroy a VAS and free the memory in use
 * @param vas The VAS to destroy
 * @returns 0 on success
 */
int vas_destroy(vas_t *vas);

/**
 * @brief Clone a VAS to a new VAS
 * @param parent The parent VAS to clone from
 * @returns A new VAS from the parent VAS
 * 
 * This also sets up a new page directory with valid mappings from the parent
 */
vas_t *vas_clone(vas_t *parent);

/**
 * @brief Get an allocation node from an allocation
 * @param vas The VAS to search in 
 * @param alloc The allocation to look for
 * @returns The node or NULL
 */
vas_node_t *vas_getFromAllocation(vas_t *vas, vas_allocation_t *alloc);

#endif