/**
 * @file userspace/essence/input.c
 * @brief Essence input system
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "essence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CSR_SHOW() { putchar('\030'); fflush(stdout); }
#define CSR_HIDE() { printf("\b"); fflush(stdout); }
#define PUTCHAR_FLUSH(c) { putchar(c); fflush(stdout); }


#define DEFAULT_BUFSIZE 128

/**
 * @brief Get a fully processed line of input
 */
char *essence_getInput() {
    size_t bufsz = DEFAULT_BUFSIZE;
    char *buffer = malloc(bufsz);
    memset(buffer, 0, bufsz);

    // Read from buffer
    ssize_t r = 0;
    while ((r = read(STDIN_FILENO, buffer, BUFSIZ)) <= 0);
    buffer[r-1] = 0;

    return buffer;
}