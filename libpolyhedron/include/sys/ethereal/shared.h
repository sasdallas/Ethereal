/**
 * @file libpolyhedron/include/sys/ethereal/shared.h
 * @brief Ethereal shared memory API (because POSIX's is trash)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_ETHEREAL_SHARED_H
#define _SYS_ETHEREAL_SHARED_H

/**** INCLUDES ****/
#include <stddef.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

#define SHARED_DEFAULT      0x0     // Default shared memory
#define SHARED_READ_ONLY    0x1     // The shared memory is read-only and any attempts to map it as rw will be ignored

/**** FUNCTIONS ****/

/**
 * @brief Create a new object of shared memory
 * @param size The size of the shared memory
 * @param flags Flags to use when creating the shared memory
 * @returns A file descriptor that you can @c mmap in
 */
int shared_new(size_t size, int flags);

/**
 * @brief Get the key of a shared memory object
 * @param fd The file descriptor of the shared memory object
 * @returns The key on success or -1 on failure
 */
key_t shared_key(int fd);

/**
 * @brief Open a shared memory object by its key
 * @param key The key to use to open the shared memory object
 * @returns A nonzero file descriptor on success or -1
 */
int shared_open(key_t key);

#endif

_End_C_Header