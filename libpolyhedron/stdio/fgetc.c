/**
 * @file libpolyhedron/stdio/fgetc.c
 * @brief fgetc 
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

extern size_t __fileio_read_bytes(FILE *f, char *buf, size_t size);

int fgetc(FILE *stream) {
    char buf[1];
    if (!__fileio_read_bytes(stream, buf, 1)) return EOF;
    return buf[0];
}

char *fgets(char *s, int size, FILE *stream) {
    if (!__fileio_read_bytes(stream, s, size)) return NULL;
    return s;
}