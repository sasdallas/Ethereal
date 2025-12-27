/**
 * @file hexahedron/include/kernel/mm/vmm.h
 * @brief Hexahedron VMM interface
 * 
 * The VMM interface of Hexahedron is inspired by the Astral VMM
 * https://github.com/Mathewnd/Astral/
 * 
 * This includes: MMU + VMM API, usage of slabs, VMM contexts, and probably a few other things.
 * No direct code is taken from the Astral kernel.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_VMM_H
#define KERNEL_MM_VMM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/mm/arch_mmu.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/slab.h>
#include <kernel/mm/alloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/mutex.h>

/**** DEFINITIONS ****/

#define VM_FLAG_DEFAULT     0x0         // Default flags
#define VM_FLAG_ALLOC       0x1         // Allocate and back the pages
#define VM_FLAG_FIXED       0x2         // Address hint must be matched exactly
#define VM_FLAG_FILE        0x4         // File mapping
#define VM_FLAG_SHARED      0x8         // Shared memory mapping
#define VM_FLAG_DEVICE      0x10        // The physical memory of this mapping refers to device memory and should not be held or freed

/* VM_OP_ */
#define VM_OP_SET_FLAGS     1
#define VM_OP_FREE          2

/* Fault location */
#define VMM_FAULT_FROM_KERNEL       0
#define VMM_FAULT_FROM_USER         1

#define VMM_FAULT_PRESENT           0x0 // Set on present access
#define VMM_FAULT_NONPRESENT        0x1 // Set on non-present access
#define VMM_FAULT_READ              0x0 // Set on attempted read
#define VMM_FAULT_WRITE             0x2 // Set on attempted write
#define VMM_FAULT_EXECUTE           0x4 // Set if attempted execution

#define VMM_FAULT_RESOLVED          0
#define VMM_FAULT_UNRESOLVED        1

/* Validation constraints */
#define VMM_PTR_USER                0x01    // Usermode or kernel pointer (STRICT: Only usermode pointer)
#define VMM_PTR_STRICT              0x02    // Strict pointer validation

/**** TYPES ****/

typedef uint64_t vmm_flags_t;

typedef struct vmm_memory_range {
    // Linked list fields
    struct vmm_memory_range *next;
    struct vmm_memory_range *prev;

    uintptr_t start;                    // MMU start address
    uintptr_t end;                      // MMU end address
    vmm_flags_t vmm_flags;              // VMM flags
    mmu_flags_t mmu_flags;              // MMU flags
    fs_node_t *node;                    // Filesystem node this node maps to    
} vmm_memory_range_t;

// Concept shamelessly taken from @mathewnd (thank you)
typedef struct vmm_space {
    uintptr_t start;                    // Start of this
    uintptr_t end;                      // End of this
    vmm_memory_range_t *range;          // Range beginning
    mutex_t *mut;                       // Mutex
} vmm_space_t;

typedef struct vmm_context {
    vmm_space_t *space;                 // Default target VMM space
    mmu_dir_t *dir;                     // Directory
} vmm_context_t;

typedef struct vmm_fault_information {
    uint8_t from;                       // Where was this exception from?
    uint8_t exception_type;             // Type of exception
    uintptr_t address;                  // The address which was faulted on
} vmm_fault_information_t;

/**** VARIABLES ****/

extern vmm_context_t *vmm_kernel_context;
extern vmm_space_t *vmm_kernel_space;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the VMM
 * @param region The list of PMM regions to use
 */
void vmm_init(pmm_region_t *region);

/**
 * @brief Create a new VMM context
 */
vmm_context_t *vmm_createContext();

/**
 * @brief Get the appropriate VMM space for the address
 * @param addr The address to get the VMM space for
 */
vmm_space_t *vmm_getSpaceForAddress(void *addr);


/**
 * @brief Map VMM memory
 * @param addr The address to use
 * @param size The size to map in
 * @param vm_flags VM flags for the mapping
 * @param prot MMU protection flags 
 * 
 * The remaining arguments depend on @c vm_flags
 * 
 * @returns The address mapped or NULL on failure.
 */
void *vmm_map(void *addr, size_t size, vmm_flags_t vm_flags, mmu_flags_t prot, ...) ;

/**
 * @brief Unmap/free VMM memory
 * @param addr The address to unmap
 * @param size The size of the region to unmap
 */
void vmm_unmap(void *addr, size_t size);

/**
 * @brief Switch VMM contexts
 * @param ctx The context to switch to
 */
void vmm_switch(vmm_context_t *ctx);

/**
 * @brief Clone a previous context into a new context
 * @param ctx The context to clone
 * @returns A new context
 */
vmm_context_t *vmm_clone(vmm_context_t *ctx);

/**
 * @brief Dump all allocations in a context
 * @param context The context to dump
 */
void vmm_dumpContext(vmm_context_t *ctx);

/**
 * @brief Find a free spot in a VMM context
 * @param space The space to search
 * @param address Address hint. If NULL, ignored.
 * @param size Size required for the region
 * @returns The start of the region or NULL on failure
 */
uintptr_t vmm_findFree(vmm_space_t *space, uintptr_t address, size_t size);

/**
 * @brief Insert a new range into a VMM context
 * @param space The space to insert into
 * @param range The range to insert into the VMM context
 */
void vmm_insertRange(vmm_space_t *space, vmm_memory_range_t *range);

/**
 * @brief Create a new VMM range (doesn't add it)
 * @param start The start of the range
 * @param end The end of the range
 */
vmm_memory_range_t *vmm_createRange(uintptr_t start, uintptr_t end, vmm_flags_t vmm_flags, mmu_flags_t mmu_flags);



/**
 * @brief Destroy a VMM memory range
 * @param space The space to destroy the range in
 * @param range The range to destroy
 */
void vmm_destroyRange(vmm_space_t *space, vmm_memory_range_t *range);

/**
 * @brief Get the range containing an allocation
 * @param space The space
 * @param start Allocation start
 * @param size Size of allocation
 */
vmm_memory_range_t *vmm_getRange(vmm_space_t *space, uintptr_t start, size_t size);

/**
 * @brief Try to handle VMM fault
 * @param info Fault information
 * @returns VMM_FAULT_RESOLVED on success and VMM_FAULT_UNRESOLVED on failure
 */
int vmm_fault(vmm_fault_information_t *info);

/**
 * @brief Validate a range of memory
 * @param start Start of the range
 * @param size The size of the range to submit
 * @param flags See @c VMM_PTR
 * 
 * @warning Pointers are NOT allowed to cross from user-kernel space.
 */
int vmm_validate(uintptr_t start, size_t size, int flags);

/**
 * @brief Destroy a context
 * @param ctx The context to destroy
 */
void vmm_destroyContext(vmm_context_t *ctx);

/**
 * @brief Map MMIO memory
 * @param physical The physical address of the MMIO memory
 * @param size The size of MMIO to map
 */
uintptr_t mmio_map(uintptr_t physical, size_t size);

/**
 * @brief Unmap MMIO memory
 * @param virt Virtual address of MMIO memory
 * @param size The size of MMIO to unmap
 */
void mmio_unmap(uintptr_t virt, size_t size);

/**
 * @brief Map DMA memory
 * @param size The size of DMA memory to allocate
 * 
 * Backs with contiguous pages in memory
 */
uintptr_t dma_map(size_t size);

/**
 * @brief Unmap DMA memory
 * @param virt The virtual address of DMA to umap
 * @param size The size of DMA to unmap
 */
void dma_unmap(uintptr_t virt, size_t size);

/**
 * @brief Post-SMP hook
 */
void vmm_postSMP();

/**
 * @brief Internal function to demark pages
 * Don't even think about calling this.
 */
void vmm_freePages(vmm_memory_range_t *range, uintptr_t offset, size_t npages);

/**
 * @brief Update the virtual memory mappings
 * @param space The space to update in
 * @param start The starting address to update
 * @param size The size to update
 * @param op_type VM_OP
 * @param mmu_flags MMU flags
 */
int vmm_update(vmm_space_t *space, void *start, size_t size, int op_type, mmu_flags_t mmu_flags);

#endif