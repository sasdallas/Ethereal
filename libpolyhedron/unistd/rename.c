/**
 * @file libpolyhedron/unistd/rename.c
 * @brief rename
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <unistd.h>

int rename(const char *oldpath, const char *newpath) {
    printf("rename %s %s\n", oldpath, newpath);
    return 0;
}