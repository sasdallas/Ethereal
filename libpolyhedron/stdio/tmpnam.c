/**
 * @file libpolyhedron/stdio/tmpnam.c
 * @brief tmpnam
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <unistd.h>

static char _buffer[L_tmpnam];
static int _lastidx = 0;

char *tmpnam_r(char *s) {
    if (!s) return NULL;
    *s = 0;
    snprintf(s, L_tmpnam, P_tmpdir "%d.%d", getpid(), _lastidx++);
    return s;
}

char *tmpnam(char *s) {
    if (!s) s = _buffer;
    return tmpnam_r(s);
}