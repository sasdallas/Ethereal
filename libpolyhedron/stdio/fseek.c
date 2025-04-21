/**
 * @file libpolyhedron/stdio/fseek.c
 * @brief fseek
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

int fseek(FILE *stream, long offset, int whence) {
    // Flush everything
    // TODO: Flush EVERYTHING
    if (stream->wbuflen) fflush(stream);

    // This should handle errno
    int return_value = lseek(stream->fd, offset, whence);
    if (return_value < 0) return -1;
    return 0;
}