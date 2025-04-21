/**
 * @file libpolyhedron/stdio/std.c
 * @brief Defines FILE structures for stdin, stdout, stderr
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

/* stdout buffer - TODO, not do this? */
/* only using because malloc() initializes the stdout buffers but sometimes malloc is stupid and needs to be debugged */
static char stdout_buf[WRITE_BUFFER_SIZE];

FILE _stdin = {
    .fd = STDIN_FILE_DESCRIPTOR,
    .rbuf = NULL,
    .rbufsz = 0,
    .wbuf = NULL,
    .wbuflen = 0,
    .wbufsz = 0,
    .eof = 0,
};

FILE _stdout = {
    .fd = STDOUT_FILE_DESCRIPTOR,
    .rbuf = NULL,
    .rbufsz = 0,
    .wbuf = stdout_buf,
    .wbuflen = 0,
    .wbufsz = WRITE_BUFFER_SIZE,
    .eof = 0,
};

FILE _stderr = {
    .fd = STDERR_FILE_DESCRIPTOR,
    .rbuf = NULL,
    .rbufsz = 0,
    .wbuf = NULL,
    .wbuflen = 0,
    .wbufsz = 0,
    .eof = 0,
};

/* Pointers to these structures */
FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;