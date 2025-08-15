/**
 * @file hexahedron/klib/string/strcpy.c
 * @brief strcpy and strncpy
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


char *strcpy(char *dst, const char *src) {
    size_t l = strlen(src);
    char *p = memcpy(dst, src, l);
    *(p + l) = 0;
    return p;
}

char *strncpy(char *dst, const char *src, size_t dsize) {
    size_t src_len = strlen(src);
    size_t len = (src_len > dsize) ? dsize : src_len;
    size_t pad_amnt = (src_len > dsize) ? 0 : dsize-src_len;
    return memset(memcpy(dst, src, len)+len, 0, pad_amnt);
}
