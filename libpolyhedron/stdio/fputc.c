/**
 * @file libpolyhedron/stdio/fputc.c
 * @brief fputc and fputs
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

extern ssize_t __fileio_write_bytes(FILE *f, const char *buf, size_t size);

int fputc(int c, FILE *f) {
    char data[1] = { c };
    if (__fileio_write_bytes(f, data, 1) < 1){
	    return EOF;
    }
    return c;
}

int fputs(const char *s, FILE *f) {
    return (int)__fileio_write_bytes(f, s, strlen(s));
}
