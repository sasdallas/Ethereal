/**
 * @file libpolyhedron/string/strncpy.c
 * @brief String copy function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>

char* strncpy(char* destination_str, const char* source_str, size_t length) {
    char *destination_ptr = destination_str; // Keep a pointer for return
    
    while (length--) {
        *destination_str = *source_str;

        destination_str++;
        source_str++;
    }

    return destination_ptr;
}

char* strcpy(char* destination_str, const char* source_str) {
    char *destination_ptr = destination_str;
    
    while ( (*destination_str = *source_str) ) {
        destination_str++;
        source_str++;
    }

    return destination_ptr;
}