/**
 * @file hexahedron/klib/string/strlen.c
 * @brief strlen
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

size_t strlen(const char *s) {
    char *p = (char*)s; // Avoiding GCC complaining
    size_t ret = 0;
    while (*p) { p++; ret++; }
    return ret;
}