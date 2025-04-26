/**
 * @file hexahedron/include/kernel/mem/reference.h
 * @brief Page reference manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_REFERENCE_H
#define KERNEL_MEM_REFERENCE_H

/**** INCLUDES ****/
#include <stdint.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the reference system
 * @param bytes How many bytes we need to allocate.
 */
void ref_init(size_t bytes);

/**
 * @brief Get how many references a frame has
 * @param frame The frame to get references of
 * @returns The current count of references a frame has
 */
int ref_get(uintptr_t frame);

/**
 * @brief Set the frame reference count
 * @param frame The frame to set references of
 * @param refs The references to set
 * @returns The previous references of the frame
 */
int ref_set(uintptr_t frame, uint8_t refs);

/**
 * @brief Increment the current references of a frame
 * @param frame The frame to increment references of
 * @returns The new amount of references or -1 if a new reference cannot be allocated
 */
int ref_increment(uintptr_t frame) ;

/**
 * @brief Decrement the current references of a page
 * @param frame The frame to decrement references of
 * @returns The new amount of references or -1 if a reference cannot be decreased
 */
int ref_decrement(uintptr_t frame);


#endif