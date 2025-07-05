/**
 * @file libpolyhedron/string/strncpy.c
 * @brief String copy function
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

char* strncpy(char* destination_str, const char* source_str, size_t length) {
    char *dest = destination_str;

    size_t rem = length;
    while (rem) {
        if (!(*source_str)) break;
        *dest = *source_str;
        dest++;
        source_str++;
        rem--;
    }

    while (rem) {
        *dest = 0;
        rem--;
        dest++;
    }

    return dest;
}

char* strcpy(char* destination_str, const char* source_str) {
    char *destination_ptr = destination_str;
    
    while ( (*destination_str = *source_str) ) {
        destination_str++;
        source_str++;
    }

    return destination_ptr;
}