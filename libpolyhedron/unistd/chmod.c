/**
 * @file libpolyhedron/unistd/chmod.c
 * @brief chmod
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

int chmod(const char *path, mode_t mode) {
    fprintf(stderr, "chmod: stub\n");
    abort();
}

int fchmod(int fildes, mode_t mode) {
    fprintf(stderr, "fchmod: stub\n");
    abort();
}