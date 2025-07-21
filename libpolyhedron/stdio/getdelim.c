/**
 * @file libpolyhedron/stdio/getdelim.c
 * @brief getdelim
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

ssize_t getdelim(char** lineptr, size_t *n, int delim, FILE *stream) {
    if (!n || !lineptr) {

    #ifndef __LIBK
        errno = EINVAL;
    #endif
    
        return -1;
    }
    
    size_t size = 32;
    if (*lineptr) {
        size = *n;
    } else {
        *lineptr = malloc(32);
    }

    size_t idx = 0;

    while (1) {
        // Get character
        int ch = fgetc(stream);

        if (ch == EOF) {
            // EOF before delimeter
            (*lineptr)[idx] = 0;
            *n = idx;

            // Error?
            if (ferror(stream) || !idx) return -1;
            return idx;
        }

        // Copy the character to the stream
        (*lineptr)[idx++] = ch;

        // Need to expand buffer?
        if (idx >= size) {
            size *= 2;
            *lineptr = realloc(*lineptr, size);
            if (!*lineptr) return -1;
        }

        if (ch == delim) {
            (*lineptr)[idx] = 0;
            *n = idx;
            return idx;
        }
    }
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    return getdelim(lineptr, n, '\n', stream);
}