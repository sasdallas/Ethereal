/**
 * @file hexahedron/task/syscalls/mkdir.c
 * @brief mkdir
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>

long sys_mkdirat(int dirfd, const char *pathname, mode_t mode) {
    vfs_inode_t *at = NULL;
    if (dirfd != AT_FDCWD) {
        vfs_file_t *f = GET_FD_OR_ERROR(dirfd);
        at = f->inode;

        int ret = vfs_mkdirat(at, (char*)pathname, mode & ~current_cpu->current_process->umask, NULL);
        FD_FINISH(f);
        return ret;
    }

    return vfs_mkdirat(at, (char*)pathname, mode & ~current_cpu->current_process->umask, NULL);
}
