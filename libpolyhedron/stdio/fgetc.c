/**
 * @file libpolyhedron/stdio/fgetc.c
 * @brief fgetc 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart, 
 * Copyright (C) 2025 Stanislas Orsola
 */

#include <stdio.h>

extern ssize_t __fileio_read_bytes(FILE *f, char *buf, size_t size);

int fgetc(FILE *stream) {
    char buf[1];
    if (__fileio_read_bytes(stream, buf, 1) < 1) return EOF;
    return buf[0];
}

char *fgets(char *s, int size, FILE *stream) {
    int c;

    char *p = s;
    int remaining = size;
    while ((c = fgetc(stream)) > 0 && remaining) {
        *p++ = c;
        remaining--;
        if (c == '\n') break;
    }

    if (remaining > 1) *p = '\0';

    // Return NULL if we didn't get anything
    if (remaining == size) return NULL;   
    
    return s;
}
