/**
 * @file libpolyhedron/stdio/fgetpos.c
 * @brief fgetpos and fsetpos
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

int fgetpos(FILE *stream, fpos_t *pos) {
    long ret = ftell(stream);
    if (ret == EOF) return EOF;

    // Set position
    *pos = ret;
    return 0;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
    return fseek(stream, *pos, SEEK_SET);
}