/**
 * @file libpolyhedron/stdio/fread.c
 * @brief fread
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

extern ssize_t __fileio_read_bytes(FILE *f, char *buf, size_t size);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!size || !nmemb || !stream) return 0;

    char *p = (char*)ptr;
    ssize_t r = __fileio_read_bytes(stream, p, nmemb*size);
    if (r == -1) {
        return -1;
    }

    // TODO: FIX!!
    if (r < (ssize_t)nmemb*(ssize_t)size) {
        return (r/size);
    }

    return nmemb; 
}
