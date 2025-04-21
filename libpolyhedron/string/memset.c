/**
 * @file libpolyhedron/string/memset.c
 * @brief memset() function
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


#ifndef MEMSET_DEFINED

void* memset(void* destination_ptr, int value, size_t size) {
    size_t i = 0;
    for (; i < size; i++) {
        ((char*)destination_ptr)[i] = value;
    }
    
    return destination_ptr;
}

#endif