/**
 * @file hexahedron/task/syscalls/readlink.c
 * @brief readlink
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

ssize_t sys_readlink(const char *path, char *buf, size_t bufsiz) {
    SYSCALL_VALIDATE_PTR(path);
    SYSCALL_VALIDATE_PTR_SIZE(buf, bufsiz);

    vfs_inode_t *i;
    int r = vfs_lookup((char*)path, &i, LOOKUP_NO_FOLLOW);
    if (r != 0) return r;
    ssize_t b = inode_readlink(i, buf, bufsiz);
    inode_release(i);
    return b;
}
