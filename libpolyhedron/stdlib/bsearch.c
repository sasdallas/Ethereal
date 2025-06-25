/**
 * @file libpolyhedron/stdlib/bsearch.c
 * @brief bsearch
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>

void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*compar)(const void *, const void *)) {
	if (!nel) return NULL;

	// Iterate through each element, incrementing by width
	const char *a = base;

	size_t idx = 0;
	while (idx < nel) {
		// Check comparison
		if (!compar(key, (const void *)a)) return (void*)a;

		idx++;
		a += width; // Next element
	}

	return NULL;
}