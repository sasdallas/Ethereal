/**
 * @file libpolyhedron/string/strlen.c
 * @brief String length function
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

size_t strlen(const char *str) {
    size_t length = 0;
    while (*str++)
        length++;
    return length;
}