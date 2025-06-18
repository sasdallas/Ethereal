/**
 * @file libpolyhedron/stdio/puts.c
 * @brief puts
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 * Copyright (C) 2025 Stanislas Orsola
 */

#include <stdio.h>
#include <string.h>

int puts(const char *s) {
    if(!fwrite(s, 1, strlen(s), stdout))return EOF;
    if(!fwrite("\n", 1, 1, stdout))return EOF;
    return 0;
}
