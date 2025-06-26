/**
 * @file libpolyhedron/stdio/fwrite.c
 * @brief fwrite
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart,
 * Copyright (C) 2025 Stanislas Orsola,
 */

#include <stdio.h>

extern ssize_t __fileio_write_bytes(FILE *f, const char *buf, size_t size);

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f) {
    if (!size || !nmemb || !f) return 0;

    // fwrite writes nmemb objects of size size to f
    char *p = (char*)ptr; // lol, screw const
    for (size_t i = 0; i < nmemb; i++) {
        ssize_t r = __fileio_write_bytes(f, p, size);
        if (r < (ssize_t)size) return i; // Out of objects
        p += r;
    }

    return nmemb;
}
