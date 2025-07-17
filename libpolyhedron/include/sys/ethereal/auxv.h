/**
 * @file libpolyhedron/include/sys/ethereal/auxv.h
 * @brief auxv for Ethereal
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

#ifndef _SYS_ETHEREAL_AUXV_H
#define _SYS_ETHEREAL_AUXV_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** TYPES ****/

typedef struct __auxv {
    uintptr_t   tls;                // TLS location in memory
    size_t      tls_size;           // TLS size in memory
} __auxv_t;


#endif