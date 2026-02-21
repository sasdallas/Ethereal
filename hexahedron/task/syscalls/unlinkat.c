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
    if (flags & AT_REMOVEDIR) { SYSCALL_LOG(WARN, "rmdir not supported\n"); return 1; }

    vfs_inode_t *at = NULL;
    if (*path == '/') {
        // Absolute path
    } else if (dirfd == AT_FDCWD) {
        at = current_cpu->current_process->wd_node;
    } else {
        if (!FD_VALIDATE(current_cpu->current_process, dirfd)) return -EBADF;
        at = FD(current_cpu->current_process,dirfd)->node->inode;
    }

    if (at) inode_hold(at);
    int ret = vfs_unlinkat(at, (char*)path);
    if (at) inode_release(at);

    return ret;
}