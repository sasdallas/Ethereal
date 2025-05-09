/**
 * @file libpolyhedron/stdio/fileio.c
 * @brief Internal file I/O operations
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
#include <stdlib.h>
#include <unistd.h>

/**
 * @brief Write file bytes
 * @param f The file object to write to
 * @param buf The buffer to write
 * @param size The size of the buffer to write
 */
size_t __fileio_write_bytes(FILE *f, char *buf, size_t size) {
    if (!f) return 0;

    // Allocate a write buffer if needed
    if (!f->wbuf) {
        f->wbuf = malloc(WRITE_BUFFER_SIZE);
        f->wbufsz = WRITE_BUFFER_SIZE;
    }

    // Now let's sequentially write data to the file
    for (size_t i = 0; i < size; i++) {
        f->wbuf[f->wbuflen++] = buf[i];

        // Do we need to flush?
        if (f->wbuflen >= f->wbufsz || (buf[i] == '\n')) {
            fflush(f);
        }
    }

    return size;
}

/**
 * @brief Read file bytes
 * @param f The file object to read from
 * @param buf The buffer to use
 * @param size The size of the buffer to use
 */
size_t __fileio_read_bytes(FILE *f, char *buf, size_t size) {
    if (!f) return 0;

    // ungetc()
    if (f->ungetc != EOF) {
        buf[0] = f->ungetc;
        f->ungetc = EOF;
        buf++;
        size--;
        if (size == 0) return 1;
    }

    // screw it, just read directly from the buffer
    // TODO: This is not good. We should probably be using f->readbuf for buffered I/O (needed as well for ungetc)
    return read(f->fd, buf, size);
}