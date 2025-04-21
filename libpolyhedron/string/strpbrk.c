/**
 * @file libpolyhedron/string/strpbrk.c
 * @brief strpbrk
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

char *strpbrk(const char *str1, const char *str2) {
    char *ptr = (char*)str1;
    ptr += strcspn(str1, str2);
    return (*ptr) ? ptr : 0; // Return NULL, not pointer to NULL byte.
}

