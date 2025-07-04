/**
 * @file libpolyhedron/unistd/ttyname.c
 * @brief ttyname 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>

char *ttyname(int fd) {
    fprintf(stderr, "ttyname: stub\n");
    return "ttyname";
}