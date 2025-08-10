/**
 * @file libpolyhedron/unistd/link.c
 * @brief link
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
#include <stdlib.h>

int link(const char *oldpath, const char *newpath) {
    fprintf(stderr, "link: %s -> %s\n", oldpath, newpath);
    assert(0);
    return 0;
}