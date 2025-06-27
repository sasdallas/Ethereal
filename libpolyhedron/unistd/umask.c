/**
 * @file libpolyhedron/unistd/umask.c
 * @brief umask
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

mode_t umask(mode_t cmask) {
    fprintf(stderr, "umask: stub\n");
    return 0;
}