/**
 * @file hexahedron/task/syscalls/symlinkat.c
 * @brief symlinkat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <stdio.h>

long sys_symlinkat(const char *target_path, int dirfd, const char *link_path) {
    vfs_inode_t *at = NULL;
    if (dirfd != AT_FDCWD) {
        vfs_file_t *f = GET_FD_OR_ERROR(dirfd);
        at = f->inode;

        int ret = vfs_symlinkat(at, (char*)target_path, (char*)link_path, NULL);
        FD_FINISH(f);
        return ret;
    }

    return vfs_symlinkat(at, (char*)target_path, (char*)link_path, NULL);
}
