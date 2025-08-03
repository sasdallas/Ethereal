/**
 * @file libpolyhedron/wchar/wcscmp.c
 * @brief wcscmp
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <wchar.h>

int wcscmp(const wchar_t *str1, const wchar_t *str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }

    return *str1 - *str2;
}

int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n) {
	if (n == 0) return 0;

	while (n-- && (*s1 == *s2)) {
		if (!n || !*s1) break;
		s1++;
		s2++;
	}
	
	return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}