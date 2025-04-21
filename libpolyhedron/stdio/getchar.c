/**
 * @file libpolyhedron/stdio/getchar.c
 * @brief getchar
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

int getc(FILE *stream) {
    return fgetc(stream);
}

int getchar() {
    return getc(stdin);
}
