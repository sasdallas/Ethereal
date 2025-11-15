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
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/mem/vas.h>
#include <kernel/misc/util.h>
#include <kernel/misc/args.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/processor_data.h>
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
    STUB();
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
    STUB();
}

/**
 * @brief Allocate memory in a VAS
 * @param vas The VAS to reserve allocations in
 * @param size The amount of memory to allocate
 * @returns The allocation or NULL if no memory could be found
 */
vas_allocation_t *vas_allocate(vas_t *vas, size_t size) {
    STUB();
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
        case VAS_ALLOC_SIGNAL_TRAMP:
            return "TRAMP  ";
        default:
            return "???????";
    }
}

/**
 * @brief Free memory in a VAS
 * @param vas The VAS to free the memory in
 * @param node The node to free
 * @param mem_freed Whether to actually free the memory, or whether it was freed/unmapped already
 * @returns 0 on success or 1 on failure
 */
int vas_free(vas_t *vas, vas_node_t *node, int mem_freed) {
    STUB();
}

/**
 * @brief Get an allocation from a VAS
 * @param vas The VAS to get the allocation from
 * @param address The address to find the allocation for
 * @returns The node of the allocation or NULL if it could not be found
 */
vas_node_t *vas_get(vas_t *vas, uintptr_t address) {
    STUB();
}

/**
 * @brief Destroy a VAS and free the memory in use
 * @param vas The VAS to destroy
 * @returns 0 on success
 */
int vas_destroy(vas_t *vas) {
    STUB();
}

/**
 * @brief Handle a VAS fault and give the memory if needed
 * @param vas The VAS
 * @param address The address the fault occurred at
 * @param size If the allocation spans over a page, this will be used as a hint for the amount to actually map in (for speed)
 * @returns 1 on fault resolution
 */
int vas_fault(vas_t *vas, uintptr_t address, size_t size) {
    STUB();
}

/**
 * @brief Dump VAS information
 */
void vas_dump(vas_t *vas) {
    STUB();
}

/**
 * @brief Get an allocation node from an allocation
 * @param vas The VAS to search in 
 * @param alloc The allocation to look for
 * @returns The node or NULL
 */
vas_node_t *vas_getFromAllocation(vas_t *vas, vas_allocation_t *alloc) {
    STUB();
}

/**
 * @brief Create a copy of an allocation
 * @param vas The VAS to create the copy in
 * @param parent_vas The parent VAS to make a copy in
 * @param source The source allocation to copy
 * @returns A new allocation
 */
vas_allocation_t *vas_copyAllocation(vas_t *vas, vas_t *parent_vas, vas_allocation_t *source) {
    STUB();
}

/**
 * @brief Clone a VAS to a new VAS
 * @param parent The parent VAS to clone from
 * @returns A new VAS from the parent VAS
 * 
 * This also sets up a new page directory with valid mappings from the parent
 */
vas_t *vas_clone(vas_t *parent) {
    STUB();
}