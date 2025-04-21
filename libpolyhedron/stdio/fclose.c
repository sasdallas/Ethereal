/**
 * @file libpolyhedron/stdio/fclose.c
 * @brief fclose
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>

int fclose(FILE *stream) {
    // Close the file descriptor stream
    if (close(stream->fd) < 0) return EOF;
    if (stream->rbuf) free(stream->rbuf);
    if (stream->wbuf) free(stream->wbuf);

    if (stream == stdin || stream == stdout || stream == stderr) {
        return 0; // We can't free the stream
    }

    free(stream);
    return 0;
}