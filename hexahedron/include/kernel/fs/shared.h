/**
 * @file hexahedron/include/kernel/fs/shared.h
 * @brief Ethereal shared memory API
 * 
 * See @c sys/ethereal/shared.h for an API description
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_SHARED_H
#define KERNEL_FS_SHARED_H

/**** INCLUDES ****/
#include <stdint.h>
#include <ethereal/shared.h>
#include <kernel/task/process.h>

/**** DEFINITIONS ****/

#define SHARED_IMPL     0x04593021  // node->impl, TODO: find a better way to identify shared memory objects (?)

/**** TYPES ****/

typedef struct shared_object {
    key_t key;              // Key of the shared memory object
    size_t size;            // Size of the shared memory object
    int flags;              // Flags of the shared memory object
    int refcount;           // References
    uintptr_t *blocks;      // Array of PMM blocks that will get mapped into memory
} shared_object_t;

/**** FUNCTIONS ****/

#ifdef __KERNEL__

/**
 * @brief Initialize shared memory system
 */
void shared_init();

/**
 * @brief Create a new shared memory object
 * @param proc The process to make a shared memory object on
 * @param size The size of the shared memory object
 * @param flags Flags for the shared memory object
 * @returns A file descriptor for the new shared memory object
 */
int sharedfs_new(process_t *proc, size_t size, int flags);

/**
 * @brief Get the key of a shared memory object
 * @param node The node of the shared memory object
 * @returns A key or errno
 */
key_t sharedfs_key(fs_node_t *node);

/**
 * @brief Open an object of shared memory by the key
 * @param proc The process opening the object
 * @param key The key to use to open the shared memory object
 * @returns A file descriptor for the new shared memory object
 */
int sharedfs_openFromKey(process_t *proc, key_t key);


#endif

#endif