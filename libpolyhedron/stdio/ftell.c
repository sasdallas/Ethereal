/**
 * @file libpolyhedron/stdio/ftell.c
 * @brief ftell
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
#include <errno.h>
#include <unistd.h>

long ftell(FILE *stream) {
    // Flush everything
    if (stream->wbuflen) fflush(stream);
    // TODO: Flush other variables. Remember to come back here when a proper reading API is done

    // This should take care of errno
    long seek_result = lseek(stream->fd, 0, SEEK_CUR);
    return seek_result;
}