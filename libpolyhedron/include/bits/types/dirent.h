/**
 * @file libpolyhedron/include/bits/types/dirent.h
 * @brief Directory entry
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

#ifndef _BITS_TYPES_DIRENT_H
#define _BITS_TYPES_DIRENT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** TYPES ****/

struct dirent {
    ino_t   d_ino;              // Inode number
    off_t   d_off;              // Not an offset
    unsigned short d_reclen;    // Record length
    unsigned char d_type;       // Type of file
    char d_name[256];           // Null-terminated filename
};

#endif

_End_C_Header