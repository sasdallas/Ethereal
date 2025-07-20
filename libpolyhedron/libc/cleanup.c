/**
 * @file libpolyhedron/libc/cleanup.c
 * @brief libc cleanup code
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


void __stdio_flush_buffers() {
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);
}

void __libc_cleanup() {
    __stdio_flush_buffers();
}