/**
 * @file libpolyhedron/stdio/putchar.c
 * @brief putchar() functions
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


/**
 * @brief Put character function
 */
int putchar(int ch) {
    return fputc(ch, stdout);
}

int putc(int c, FILE *stream) {
    return fputc(c, stream);
}