/**
 * @file libpolyhedron/include/iconv.h
 * @brief icnv
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

#ifndef _ICONV_H
#define _ICONV_H

/**** INCLUDES ****/
#include <stddef.h>

/**** TYPES ****/
typedef void* iconv_t;

/**** FUNCTIONS ****/

size_t	iconv(iconv_t cd, char** __restrict inbuf, size_t* __restrict inbytesleft, char** __restrict outbuf, size_t* __restrict outbytesleft);
int		iconv_close(iconv_t cd);
iconv_t	iconv_open(const char* tocode, const char* fromcode);

#endif

_End_C_Header