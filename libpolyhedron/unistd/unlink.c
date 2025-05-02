/**
 * @file libpolyhedron/unistd/unlink.c
 * @brief unlink
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

int unlink(const char *pathname) {
    fprintf(stderr, "libc: Unimplemented method \"unlink\": %s\n", pathname);
    errno = EROFS;
    return -1;
}