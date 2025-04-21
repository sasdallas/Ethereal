/**
 * @file libpolyhedron/string/strncat.c
 * @brief strncat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stddef.h>
#include <string.h>

char * strncat(char *dest, const char *src, size_t n) {
	
	// Get the end of the dest ptr
	char *end = dest;
	while (*end != '\0') {
		end++;
	}

	size_t i = 0;

	while (*src && i < n) {
		*end = *src;
		end++;
		src++;
		i++;
	}
	*end = '\0';
	return dest;
}