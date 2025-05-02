/**
 * @file libpolyhedron/unistd/remove.c
 * @brief remove
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


int remove(const char *pathname) {
    printf("remove %s\n", pathname);
    errno = EROFS;
    return -1;
}