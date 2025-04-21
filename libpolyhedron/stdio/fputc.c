/**
 * @file libpolyhedron/stdio/fputc.c
 * @brief fputc and fputs
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

extern size_t __fileio_write_bytes(FILE *f, char *buf, size_t size);

int fputc(int c, FILE *f) {
    char data[1] = { c };
    __fileio_write_bytes(f, data, 1);
    return c;
}

int fputs(const char *s, FILE *f) {
    while (*s) fputc(*s, f);
    return 0;
}