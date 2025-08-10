/**
 * @file libpolyhedron/include/sys/libc_debug.h
 * @brief libc debug
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

#ifndef _SYS_LIBC_DEBUG_H
#define _SYS_LIBC_DEBUG_H

/**** INCLUDES ****/
#include <stdlib.h>
#include <stdio.h>

/**** DEFINITIONS ****/

#define LIBC_DEBUG_ENV          "__LIBC_DEBUG"

/**** MACROS ****/
#define dprintf(...) if (__libc_debug_enabled()) fprintf(stderr, __VA_ARGS__)

/**** FUNCTIONS ****/

int __libc_debug_enabled();

#endif

_End_C_Header