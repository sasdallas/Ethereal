/**
 * @file libpolyhedron/stdio/setvbuf.c
 * @brief setvbuf
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    return 0;
}

void setbuf(FILE *stream, char *buf) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, WRITE_BUFFER_SIZE); // !!!
}

void setbuffer(FILE *stream, char *buf, size_t size) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

void setlinebuf(FILE *stream) {
    setvbuf(stream, NULL, _IOLBF, 0);
}