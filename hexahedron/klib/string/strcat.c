/**
 * @file hexahedron/klib/string/strcat.c
 * @brief strcat
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

char *strcat(char *dst, const char *src) {
    size_t s_len = strlen(src);
    *(((unsigned char*)memcpy(dst + strlen(dst), src, s_len)) + s_len) = 0;
    return dst;
}

char *strncat (char *dst, const char *src, size_t n) {
    size_t s_len = strlen(src);
    if (s_len > n) s_len = n;
    *(((unsigned char*)memcpy(dst + strlen(dst), src, s_len)) + s_len) = 0;
    return dst;
}