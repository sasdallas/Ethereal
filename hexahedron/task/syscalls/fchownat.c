/**
 * @file hexahedron/task/syscalls/fchownat.c
 * @brief fchownat
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
#include <kernel/fs/vfs_new.h>

long sys_fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags) {
    // flags can be AT_EMPTY_PATH or AT_SYMLINK_NOFOLLOW
    int open_flags = O_RDWR;
    if (flags & AT_SYMLINK_NOFOLLOW) open_flags |= O_NOFOLLOW;

    vfs_file_t *f = NULL;
    if (path[0] == '/' || dirfd == AT_FDCWD) {
        // this opens either relative to cwd or absolute anyways,
        // so both cases are acceptible
        int r = vfs_open((char*)path, open_flags, &f);
        if (r != 0) return r;
    } else if (path[0] == 0) {
        // path is an empty string, if AT_EMPTY_PATH was specified this means that dirfd IS the file
        if ((flags & AT_EMPTY_PATH) == 0) return -ENOENT;
        f = GET_FD_OR_ERROR(dirfd);
    } else {
        // otherwise, open relative to dirfd
        vfs_file_t *at = GET_FD_OR_ERROR(dirfd);
        int r = vfs_openat(at->inode, (char*)path, open_flags, &f);
        if (r != 0) {
            FD_FINISH(at);
            return r;
        }
        FD_FINISH(at);
    }

    // try it
    int r = vfs_chown(f->inode, uid, gid);

    // release the file
    if (path[0] == 0) {
        FD_FINISH(f);
    } else {
        vfs_close(f);
    }

    return r;
}
