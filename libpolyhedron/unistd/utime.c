/**
 * @file libpolyhedron/unistd/utime.c
 * @brief utime
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

int utime(const char *filename, void *utimbuf) {
    fprintf(stderr, "utime: stub\n");
    abort();
}