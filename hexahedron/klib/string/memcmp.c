/**
 * @file hexahedron/klib/string/memcmp.c
 * @brief memcmp
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

int memcmp(const void *s1, const void *s2, size_t n) {
    unsigned char *p1 = (unsigned char *)s1;
    unsigned char *p2 = (unsigned char *)s2;
    size_t r = n;

    while (r) {
        if (*p1 != *p2) {
            return (*p1 - *p2);
        }

        p1++;
        p2++;
        r--;
    }

    return 0;
}