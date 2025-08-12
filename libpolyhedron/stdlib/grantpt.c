/**
 * @file libpolyhedron/stdlib/grantpt.c
 * @brief grantpt
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
#include <sys/libc_debug.h>

int grantpt(int fd) {
    dprintf("libc: grantpt: stub function\n");
    return 0;
}