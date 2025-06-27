/**
 * @file libpolyhedron/unistd/pathconf.c
 * @brief pathconf
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

long fpathconf(int fd, int name) {
    fprintf(stderr, "fpathconf: stub\n");
    return 0;
}

long pathconf(const char *path, int name) {
    fprintf(stderr, "pathconf: stub\n");
    return 0;
}