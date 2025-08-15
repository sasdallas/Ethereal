/**
 * @file hexahedron/klib/string/memchr.c
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

void *memchr(const void *s, int ch, size_t n) {
    unsigned char *p = (unsigned char*)s;
    for (unsigned i = 0; i < n; i++) if (p[i] == ch) return &p[i];
    return NULL;
}

void *memchrr(const void *s, int ch, size_t n) {
    unsigned char *p = (unsigned char*)s;
    for (unsigned i = n; i > 0; i--) if (p[i] == ch) return &p[i];
    return NULL;
}