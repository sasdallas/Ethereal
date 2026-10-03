/**
 * @file hexahedron/task/syscalls/unlinkat.c
 * @brief unlinkat
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
#include <kernel/fs/vfs_new.h>
#include <unistd.h>

long sys_unlinkat(int dirfd, const char *path, int flags) {
    SYSCALL_VALIDATE_PTR(path);

    vfs_inode_t *at = NULL;
    if (*path == '/') {
        // Absolute path
    } else if (dirfd == AT_FDCWD) {
        at = current_cpu->current_process->wd_node;
    } else {
        if (!FD_VALIDATE(dirfd)) return -EBADF;
        at = FD(dirfd)->inode;
    }


    SYSCALL_LOG(INFO, "unlinkat at=%p dirfd=%d path=%s flags=%d\n", at, dirfd, path, flags);
    if (at) inode_hold(at);

    int ret;
    if (flags & AT_REMOVEDIR) {
        ret = vfs_rmdirat(at, (char*)path);
    } else {
        ret = vfs_unlinkat(at, (char*)path);
    }
    if (at) inode_release(at);


    return ret;
}
