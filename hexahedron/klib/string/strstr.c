/**
 * @file hexahedron/klib/string/strstr.c
 * @brief strstr
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <string.h>
#include <assert.h>

char* strstr(const char* haystack, const char* needle) {
    size_t l = strlen(needle);
    if (l == 0) return (char*)haystack;

    for (size_t i = 0; haystack[i]; i++) {
		if (strncmp(haystack + i, needle, l) == 0) {
            return ((char*)haystack + i);
		}
	}
        
    return NULL;
}