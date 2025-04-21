/**
 * @file libpolyhedron/string/memcmp.c
 * @brief memcmp function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>


int memcmp(const void* aptr, const void* bptr, size_t n) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	
    for (size_t i = 0; i < n; i++) {
		if (a[i] < b[i]) return -1;
		else if (b[i] < a[i]) return 1;
	}

	return 0;
}
