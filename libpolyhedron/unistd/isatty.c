/**
 * @file libpolyhedron/unistd/isatty.c
 * @brief isatty
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>

int isatty(int fd) {
    fprintf(stderr, "isatty: %d\n", fd);
    return 1;
}