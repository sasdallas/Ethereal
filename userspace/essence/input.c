/**
 * @file userspace/essence/input.c
 * @brief Essence input system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "essence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/**
 * @brief Get a fully processed line of input
 */
char *essence_getInput() {
    char *buf = malloc(1024);
    memset(buf, 0, 1024);


    // !!!: BAD
    int i = 0;
    while (1) {
        char ch = getchar();
        if (ch == '\n') {
            // Flush, we're done
            putchar(ch);
            printf("debug: Essence input reading completed: %p (%d)\n", buf, strlen(buf));
            return buf;
        }

        if (ch == '\t') {
            // Autocomplete?
            printf("essence: Autocomplete unavailable\n");
            continue;
        }

        if (ch == '\b') {
            // Backspace
            buf[i] = 0;
            i--;
        }

        buf[i] = ch;
        putchar(ch);
        fflush(stdout);
        i++;
    }

    return buf;
}