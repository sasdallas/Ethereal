/**
 * @file libpolyhedron/stdlib/unlockpt.c
 * @brief unlockpt
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

int unlockpt(int fd) {
    dprintf("libc: unlockpt: stub\n");
    return 0;
}