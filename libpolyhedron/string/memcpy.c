/**
 * @file libpolyhedron/string/memcpy.c
 * @brief memcpy() function
 * 
 * 
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2015-2021 K. Lange
 * Copyright (C) 2015      Dale Weiler
 */


// IMPORTANT:   This file is temporary. I literally have no idea how this works but it prevents errors on Bochs.
//              (result of 4h debugging)

#include <string.h>


#ifndef MEMCPY_DEFINED

void* memcpy(void* dest, const void*  src, size_t n) {
	uint64_t * d_64 = dest;
	const uint64_t * s_64 = src;

	for (; n >= 8; n -= 8) {
		*d_64++ = *s_64++;
	}

	uint32_t * d_32 = (void*)d_64;
	const uint32_t * s_32 = (const void*)s_64;

	for (; n >= 4; n -= 4) {
		*d_32++ = *s_32++;
	}

	uint8_t * d = (void*)d_32;
	const uint8_t * s = (const void*)s_32;

	for (; n > 0; n--) {
		*d++ = *s++;
	}

	return dest;
}

#endif