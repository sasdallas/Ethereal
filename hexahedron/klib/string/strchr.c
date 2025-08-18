/**
 * @file hexahedron/klib/string/strchr.c
 * @brief strchr
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

/* mlibc does not have definition */
extern void *memchrr(const void *s, int ch, size_t n);

char *strchr(const char *s, int c) {
    return memchr(s, c, strlen(s));
}

char *strrchr(const char *s, int c) {
    return memchrr(s, c, strlen(s));
}

char *strchrnul(const char *s, int c) {
    size_t l = strlen(s);
    char *p = memchr(s, c, l);
    if (!p) return (char*)s + l;
    return p;
}