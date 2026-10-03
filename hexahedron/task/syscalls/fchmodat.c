/**
 * @file hexahedron/task/syscalls/fchmodat.c
 * @brief fchmodat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#define _GNU_SOURCE
#include <kernel/task/process.h>
#include <kernel/fs/vfs_new.h>
#include <unistd.h>

long sys_fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    SYSCALL_VALIDATE_PTR(path);
    if (flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) return -EINVAL;

    if (path[0] == 0) {
        if (!(flags & AT_EMPTY_PATH)) return -ENOENT;
        vfs_file_t *file = GET_FD_OR_ERROR(dirfd);
        int r = vfs_chmod(file->inode, mode);
        FD_FINISH(file);
        return r;
    }

    uint32_t lookup_flags = LOOKUP_DEFAULT;
    if (flags & AT_SYMLINK_NOFOLLOW) lookup_flags |= LOOKUP_NO_FOLLOW;
    vfs_inode_t *inode;
    int r;
    if (path[0] == '/' || dirfd == AT_FDCWD) {
        r = vfs_lookup((char *)path, &inode, lookup_flags);
    } else {
        vfs_file_t *dir = GET_FD_OR_ERROR(dirfd);
        if (dir->inode->attr.type != VFS_DIRECTORY) {
            FD_FINISH(dir);
            return -ENOTDIR;
        }
        r = vfs_lookupat(dir->inode, (char *)path, &inode, lookup_flags);
        FD_FINISH(dir);
    }
    if (r) return r;
    r = vfs_chmod(inode, mode);
    inode_release(inode);
    return r;
}
