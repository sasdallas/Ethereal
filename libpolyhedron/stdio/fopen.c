/**
 * @file libpolyhedron/stdio/fopen.c
 * @brief fopen
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#ifndef __LIBK

FILE *fopen(const char *pathname, const char *mode) {
    // Convert the mode into something useful
    // r        Open text file for reading
    // r+       Open text file for reading and writing
    // w        Truncate file length to zero or create text file
    // w+       Open for reading and writing. Created if nonexistant, or truncated.
    // a        Open for appending
    // a+       Open for reading and appending

    int flags = 0;      // Flags
    mode_t mode_arg = 0; // Mode

    char *p = (char*)mode;
    switch (*p) {
        case 'r':
            flags |= O_RDONLY;
            mode_arg = 0644;    // Default mask
            break;
        
        case 'w':
            flags |= O_WRONLY | O_CREAT | O_TRUNC;
            mode_arg = 0666;    // Write-only mask
            break;
        
        case 'a':
            flags |= O_APPEND | O_WRONLY | O_CREAT;
            mode_arg = 0644;    // Default mask
            break;
        
        default:
            break;
    }

    p++;
    if (*p == '+') {
        // + detected, use O_RDWR and clear any bits that arent used
        flags &= ~(O_RDWR | O_APPEND);
    }

    // Okay we're good now. Ty to open the file
    int fd = open(pathname, flags, mode_arg);
    if (fd < 0) {
        // Error, don't bother. open() should set an errno
        return NULL;
    }

    // Allocate memory
    FILE *f = malloc(sizeof(FILE));
    memset(f, 0, sizeof(FILE));
    f->fd = fd;
    f->ungetc = EOF;

    // TODO: Need to keep a list of files to cleanup at the end..

    // Most of the buffers setup for f will be allocated on the fly to conserve memory.
    return f;
}

FILE *fdopen(int fildes, const char *mode) {
    // TODO: Properly do this?
    FILE *f = malloc(sizeof(FILE));
    memset(f, 0, sizeof(FILE));
    f->fd = fildes;
    f->ungetc = EOF;
    return f;
}

#endif