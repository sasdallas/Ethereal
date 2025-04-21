/**
 * @file libpolyhedron/stdio/fprintf.c
 * @brief fprintf, vfprintf, vprintf
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

static int file_print(void *user, char c) {
    fputc(c, (FILE*)user);
    return 0;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    return xvasprintf(file_print, (void*)f, fmt, ap);
}

int vprintf(const char *fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int out = vfprintf(f, fmt, ap);
    va_end(ap);
    return out;
}



