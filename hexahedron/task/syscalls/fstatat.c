/**
 * @file hexahedron/task/syscalls/fstatat.c
 * @brief fstatat
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

static int __stat_common(vfs_file_t *f, struct stat *statbuf) {
    vfs_inode_attr_t attr; 
    int r = vfs_getattr(f->inode, &attr);
    if (r) return r;

    // Convert VFS flags to st_dev

    statbuf->st_dev = 0;
    if (attr.type == VFS_DIRECTORY)      statbuf->st_dev |= S_IFDIR; // Directory
    if (attr.type == VFS_BLOCKDEVICE)    statbuf->st_dev |= S_IFBLK; // Block device
    if (attr.type == VFS_CHARDEVICE)     statbuf->st_dev |= S_IFCHR; // Character device
    if (attr.type == VFS_FILE)           statbuf->st_dev |= S_IFREG; // Regular file
    if (attr.type == VFS_SYMLINK)        statbuf->st_dev |= S_IFLNK; // Symlink
    if (attr.type == VFS_PIPE)           statbuf->st_dev |= S_IFIFO; // FIFO or not, it's a pipe
    if (attr.type == VFS_SOCKET)         statbuf->st_dev |= S_IFSOCK; // Socket

    // st_mode is just st_dev with extra steps
    statbuf->st_mode = statbuf->st_dev;

    // Setup other fields
    statbuf->st_ino = attr.ino; // Inode number
    statbuf->st_mode |= attr.mode; // File mode - TODO: Make sure that file mode is properly set with vaild mask bits
    statbuf->st_nlink = attr.nlink;
    statbuf->st_uid = attr.uid;
    statbuf->st_gid = attr.gid;
    statbuf->st_rdev = attr.rdev;
    statbuf->st_size = attr.size;
    statbuf->st_blksize = 512; // TODO: This would prove useful for file I/O
    statbuf->st_blocks = 0; // TODO
    statbuf->st_atime = attr.atime;
    statbuf->st_mtime = attr.mtime;
    statbuf->st_ctime = attr.ctime;
    return 0;
}

long sys_fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
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
    int r = __stat_common(f, buf);

    // release the file
    if (path[0] == 0) {
        FD_FINISH(f);
    } else {
        vfs_close(f);
    }

    return r;
}
