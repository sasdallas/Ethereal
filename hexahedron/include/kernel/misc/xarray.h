/**
 * @file hexahedron/include/kernel/misc/xarray.h
 * @brief XArray implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_MISC_XARRAY_H
#define KERNEL_MISC_XARRAY_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define XA_SHIFT 6
#define XA_ENTRIES (1 << XA_SHIFT)
#define XA_MASK ((1 << XA_SHIFT) - 1)

/**** TYPES ****/

typedef void *xarray_ptr_t;

typedef struct xarray_node {
    size_t shift;
    uint64_t bitmap;
    struct xarray_node *parent;
    xarray_ptr_t entries[XA_ENTRIES];
} xarray_node_t;

typedef struct xarray {
    xarray_ptr_t xa_head;
    unsigned char xa_lck; // write lock
} xarray_t;

/**** MACROS ****/

#define XA_INIT(xa) ({ (xa)->xa_head = NULL; (xa)->xa_lck = 0; })
#define XA_INITIALIZER { .xa_head = NULL, .xa_lck = 0 }

#define xa_foreach(xa, index, entry) \
    for ((entry) = xa_first((xa), &(index)); \
         (entry) != NULL; \
         (entry) = xa_next((xa), &(index)))

/**** FUNCTIONS ****/

/**
 * @brief Load an entry from an XArray
 * @param xa The XArray to load from
 * @param index The index to load
 * @returns The value at that index
 */
void *xa_load(xarray_t *xa, unsigned long index);

/**
 * @brief Store an entry into an XArray
 * @param xa The XArray to store to
 * @param index The index to store at
 * @param entry The entry to store
 * @returns The previous value at index
 * 
 * @note This function allocates memory
 */
void *xa_store(xarray_t *xa, unsigned long index, void *entry);

/**
 * @brief Delete an entry from an XArray
 * @param xa The XArray to delete from
 * @param index The index of the entry
 * @returns The previous value
 */
void *xa_erase(xarray_t *xa, unsigned long index);

/**
 * @brief Find the first occupied entry in the XArray
 * @param xa The XArray
 * @param index Output pointer for the index
 * @param max The maximum to go to
 */
void *xa_find(xarray_t *xa, unsigned long *index, unsigned long max);

/**
 * @brief Find the first occupied entry in the XArray, going backwards
 * @param xa The XArray
 * @param index The maximum index
 */
void *xa_findBackwards(xarray_t *xa, unsigned long *index);

/**
 * @brief Get the first entry in the array starting from inex
 * @param xa The XArray
 * @param index Index out
 * @returns The value at the entry or NULL
 */
void *xa_first(xarray_t *xa, unsigned long *index);

/**
 * @brief Get the next entry 
 * @param xa The XArray
 * @param index The index out
 */
void *xa_next(xarray_t *xa, unsigned long *index);

/**
 * @brief Destroy all entries in an XArray
 * @param xa The XArray to destroy
 */
void xa_destroy(xarray_t *xa);

#endif
