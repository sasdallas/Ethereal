/**
 * @file hexahedron/klib/string/strcmp.c
 * @brief strcmp, strncmp, strcasecmp, etc.
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
#include <stddef.h>
#include <strings.h>
#include <ctype.h>

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) break;
        s1++; s2++;
    }

    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *s1 - *s2;
}

int strcasecmp(const char *s1, const char *s2) {
    while (1) {
        unsigned char a = *s1++;
        unsigned char b = *s2++;
        if (((a >= 'A') && (a <= 'Z'))) a += 0x20;
        if (((b >= 'A') && (b <= 'Z'))) b += 0x20;

        if (!a && !b) {
            return 0;
        }

        if (a < b) {
            return -1;
        } else if (a > b) {
            return 1;
        }
    }
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        unsigned char a = *s1++;
        unsigned char b = *s2++;
        if (((a >= 'A') && (a <= 'Z'))) a += 0x20;
        if (((b >= 'A') && (b <= 'Z'))) b += 0x20;

        if (!a && !b) {
            return 0;
        }

        if (a < b) {
            return -1;
        } else if (a > b) {
            return 1;
        }
    }
    return 0;
}
