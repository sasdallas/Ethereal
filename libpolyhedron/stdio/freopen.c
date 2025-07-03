/**
 * @file libpolyhedron/stdio/freopen.c
 * @brief freopen
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 * Copyright (C) 2025 Stanislas Orsola,
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

FILE *freopen(const char *pathname, const char *mode, FILE *stream) {
    if (!pathname) {
        fprintf(stderr, "freopen: Cannot change mode without path - unsupported\n");
        return NULL;
    }

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
        flags &= ~(O_CREAT);
		flags |= O_RDWR;
    }

    // Close the stream file descriptor
    close(stream->fd);
    stream->fd = open(pathname, flags, mode_arg);
    if (stream->fd < 0) return NULL;
    return stream;
}
