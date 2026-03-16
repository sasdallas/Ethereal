/**
 * @file hexahedron/include/kernel/misc/hash.h
 * @brief Kernel hash algorithms
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
*/

#ifndef KERNEL_MISC_HASH_H
#define KERNEL_MISC_HASH_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** FUNCTIONS ****/
uint32_t crc32(void *ptr, size_t size);
uint32_t fnv1a_32(void *ptr, size_t size);
uint64_t fnv1a_64(void *ptr, size_t size);

#endif