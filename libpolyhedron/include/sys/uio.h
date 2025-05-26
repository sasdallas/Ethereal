/**
 * @file libpolyhedron/include/sys/uio.h
 * @brief Definitions for vector I/O operations
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

/**** INCLUDES ****/
#include <stddef.h>

/**** TYPES ****/

struct iovec {
    void    *iov_base;      // Base address of a memory region for I/O
    size_t  iov_len;        // Size of memory pointed to by iov_base
};

#endif

_End_C_Header