/**
 * @file libpolyhedron/string/memmove.c
 * @brief memmove() function
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

#ifndef __MEMMOVE_DEFINED

void* memmove(void* destination_ptr, const void* source_ptr, size_t size) {
    unsigned char* destination = (unsigned char*)destination_ptr;
    const unsigned char* source = (const unsigned char*)source_ptr;

    if (destination < source) {
        // Copy normally
        for (size_t i = 0; i < size; i++) 
            destination[i] = source[i];
    } else {
        // Copy in reverse (TODO: != 0 or > 0?)
        for (size_t i = size; i != 0; i--) 
            destination[i-1] = source[i - 1];
    }

    return destination_ptr;
}

#endif