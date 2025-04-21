/**
 * @file libpolyhedron/stdio/fflush.c
 * @brief fflush
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

#ifndef __LIBK

int fflush(FILE *f) {
    if (!f->wbuf) return EOF; // What are you trying to flush?

    if (f->wbuflen) {
        // We have length, update
        write(f->fd, f->wbuf, f->wbuflen);
        f->wbuflen = 0;
    }


    return 0;
}

#else

int fflush(FILE *f) {
    return 0;
}

#endif