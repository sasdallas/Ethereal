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


    int i = 0;  
    int c;

    while (1) {
        CSR_SHOW();
        c = getchar();

        switch (c) {
            case '\b':
                // Backspace character
                CSR_HIDE();
                if (i > 0) {
                    // Have space, go ahead
                    i--;
                    printf("\b");
                    fflush(stdout);
                }
                break;

            case '\n':
                // Newline, flush
                buffer[i] = 0;
                CSR_HIDE();
                PUTCHAR_FLUSH('\n');
                return buffer;

            default:
                putchar('\b');
                PUTCHAR_FLUSH(c);
                

                // Write char to buffer
                buffer[i] = c;
                i++;
                if ((size_t)i >= bufsz) {
                    bufsz = bufsz + DEFAULT_BUFSIZE;
                    buffer = realloc(buffer, bufsz);
                }
                break;
        }


    }

    return buffer;
}