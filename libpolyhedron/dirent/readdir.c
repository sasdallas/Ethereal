/**
 * @file libpolyhedron/dirent/readdir.c
 * @brief readdir
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

DEFINE_SYSCALL3(readdir, SYS_READDIR, struct dirent*, int, unsigned long);

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) {
    struct dirent *ent = (entry ? entry : *result);
    
    // Store pointer if needed
    if (entry) *result = ent;

    // Increase index
    unsigned long index = dirp->current_index;
    dirp->current_index++;

    // Request
    long _ret = __syscall_readdir(ent, dirp->fd, index);
        
    // Set errno
    if (_ret < 0) { 
        errno = -_ret;
        return -1;
    }

    // 1 indicates that we got an entry and set it
    if (_ret == 1) return 0;

    // Else, zero the structure
    memset(entry, 0, sizeof(struct dirent));
    return 0;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent ent; // !!!: Is this good?

    // Request
    long _ret = __syscall_readdir(&ent, dirp->fd, dirp->current_index);
        
    // Set errno
    if (_ret < 0) { 
        errno = -_ret;
        return NULL;
    }

    // Are we at the end of the directory?
    if (_ret == 1) {
        // We aren't, valid entry
        dirp->current_index++;
        return &ent;
    }

    // We are at the end, NULL
    return NULL;
}