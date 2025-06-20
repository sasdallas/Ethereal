/**
 * @file libpolyhedron/stdio/fcntl.c
 * @brief fcntl
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <fcntl.h>
#include <stdio.h>

int fcntl(int fd, int op, ...) {
    fprintf(stderr, "fcntl: %d: %d\n", fd, op);
    return 0;
}