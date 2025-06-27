/**
 * @file libpolyhedron/string/strxfrm.c
 * @brief strxfrm
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
#include <locale.h>

// TODO: Support other locales

size_t strxfrm(char *dest, const char *src, size_t n) {
    char *s = (char*)src;
    size_t idx = 0;
    while (*s) {
        if (idx == n) break;

        *dest++ = *s++;
        idx++;
    }   

    if (idx < n) *dest = 0;
    return idx;
}