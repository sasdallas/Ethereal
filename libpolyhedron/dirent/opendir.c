/**
 * @file libpolyhedron/dirent/opendir.c
 * @brief opendir
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

DIR *fdopendir(int fd) {
    DIR *dirp = malloc(sizeof(DIR));
    dirp->fd = fd;
    dirp->current_index = 0;
    return dirp;
}

DIR *opendir(const char *path) {
    int fd = open(path, O_DIRECTORY);
    if (fd < 0) return NULL;
    return fdopendir(fd);
}


