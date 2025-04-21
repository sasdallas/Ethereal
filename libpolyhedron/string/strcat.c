/**
 * @file libpolyhedron/string/strcat.c
 * @brief strcat 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>

char * strcat(char *dest, const char *src) {
	char * end = dest;
	while (*end != '\0') {
		++end;
	}
	while (*src) {
		*end = *src;
		end++;
		src++;
	}
	*end = '\0';
	return dest;
}