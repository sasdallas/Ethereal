/**
 * @file libpolyhedron/include/endian.h
 * @brief endian
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _ENDIAN_H
#define _ENDIAN_H

/**** DEFINITIONS ****/

#define LITTLE_ENDIAN       0
#define BIG_ENDIAN          1

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER          LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER          BIG_ENDIAN
#endif



#endif

_End_C_Header