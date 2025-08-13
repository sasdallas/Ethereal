/**
 * @file libpolyhedron/string/strdup.c
 * @brief strdup
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>
#include <stdlib.h>

char *strdup(const char *str) {
    char *new_str = malloc(strlen(str) + 1);
    if (!new_str) return NULL;
    memcpy(new_str, str, strlen(str) + 1);
    return new_str;
}

char *strndup(const char *str, size_t size) {
    size_t str_length = strlen(str);
    size_t actual_len = (str_length > size) ? size : str_length;

    char *p = malloc(actual_len+1);
    memcpy(p, str, actual_len);
    p[actual_len] = 0;

    return p;
}