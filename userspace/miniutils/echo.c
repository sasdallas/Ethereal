/**
 * @file userspace/miniutils/echo.c
 * @brief echo
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal operating system.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);    
    }

    putchar('\n');
    return 0;
}