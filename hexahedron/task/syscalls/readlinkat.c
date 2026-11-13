/**
 * @file hexahedron/task/syscalls/readlinkat.c
 * @brief readlinkat
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

ssize_t sys_readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
    SYSCALL_VALIDATE_PTR(path);
    SYSCALL_VALIDATE_PTR_SIZE(buf, bufsiz);

    vfs_inode_t *dir = NULL;
    if (*path == '/') {
        // Absolute path
    } else if (dirfd == AT_FDCWD) {
        dir = current_cpu->current_process->wd_node;
    } else {
        if (!FD_VALIDATE(dirfd)) return -EBADF;
        dir = FD(dirfd)->inode;
    }

    if (dir) inode_hold(dir);

    vfs_inode_t *i;
    int r = vfs_lookupat(dir, (char*)path, &i, LOOKUP_NO_FOLLOW);
    if (r != 0) {
        if (dir) inode_release(dir);
        return r;
    }

    if (i->attr.type != VFS_SYMLINK) {
        inode_release(i);
        if (dir) inode_release(dir);
        return -EINVAL;
    }

    ssize_t b = inode_readlink(i, buf, bufsiz);
    inode_release(i);
    if (dir) inode_release(dir);
    return b;
}
