/**
 * @file libpolyhedron/string/memchr.c
 * @brief memchr
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

void *memchr(const void *ptr, int ch, size_t count) {
    unsigned char *p = (unsigned char*)ptr;
    size_t idx = 0;

    while (idx < count) {
        if (*p == ch) return p;
        p++;
        idx++;
    }

    return NULL;
}