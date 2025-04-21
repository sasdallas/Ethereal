/**
 * @file libpolyhedron/string/strncasecmp.c
 * @brief strncasecmp 
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
#include <ctype.h>
#include <strings.h>

int strncasecmp(const char *s1, const char *s2, size_t n) {
	if (n == 0) return 0;

	while (n-- && tolower(*s1) == tolower(*s2)) {
		if (!n || !*s1) break;
		s1++;
		s2++;
	}
	return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}