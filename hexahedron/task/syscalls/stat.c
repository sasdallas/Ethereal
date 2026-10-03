/**
 * @file hexahedron/task/syscalls/stat.c
 * @brief stat and friends
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
#include <string.h>

static int sys_stat_common(vfs_file_t *f, struct stat *statbuf) {
    vfs_inode_attr_t attr; 
    int r = vfs_getattr(f->inode, &attr);
    if (r) return r;

    memset(statbuf, 0, sizeof(struct stat));

    statbuf->st_dev = f->inode->mount ? f->inode->mount->dev : 0;

    statbuf->st_mode = 0;
    if (attr.type == VFS_DIRECTORY)      statbuf->st_mode |= S_IFDIR;
    if (attr.type == VFS_BLOCKDEVICE)    statbuf->st_mode |= S_IFBLK;
    if (attr.type == VFS_CHARDEVICE)     statbuf->st_mode |= S_IFCHR;
    if (attr.type == VFS_FILE)           statbuf->st_mode |= S_IFREG;
    if (attr.type == VFS_SYMLINK)        statbuf->st_mode |= S_IFLNK;
    if (attr.type == VFS_PIPE)           statbuf->st_mode |= S_IFIFO;
    if (attr.type == VFS_SOCKET)         statbuf->st_mode |= S_IFSOCK;

    // Setup other fields
    statbuf->st_ino = attr.ino; // Inode number
    statbuf->st_mode |= attr.mode;
    statbuf->st_nlink = attr.nlink;
    statbuf->st_uid = attr.uid;
    statbuf->st_gid = attr.gid;
    statbuf->st_rdev = attr.rdev;
    statbuf->st_size = attr.size;
    statbuf->st_blksize = 512; // TODO: This would prove useful for file I/O
    statbuf->st_blocks = 0; // TODO
    statbuf->st_atim.tv_sec = attr.atime;
    statbuf->st_atim.tv_nsec = 0;
    statbuf->st_mtim.tv_sec = attr.mtime;
    statbuf->st_mtim.tv_nsec = 0;
    statbuf->st_ctim.tv_sec = attr.ctime;
    statbuf->st_ctim.tv_nsec = 0;

    return 0;
}

long sys_stat(const char *pathname, struct stat *statbuf) {
    vfs_file_t *f;
    int r = vfs_open((char*)pathname, 0, &f);
    if (r) return r;

    r = sys_stat_common(f, statbuf);
    if (r) { vfs_close(f); return r; }

    vfs_close(f);

    return 0;
}

long sys_fstat(int fd, struct stat *statbuf) {
    if (!FD_VALIDATE(fd)) return -EBADF;

    return sys_stat_common(FD(fd), statbuf);
}

long sys_lstat(const char *pathname, struct stat *statbuf) {
    // Try to open the file
    vfs_file_t *f;
    int r = vfs_open((char*)pathname, O_NOFOLLOW | O_PATH, &f);
    if (r) {
        return r;
    }

    // Common stat
    r = sys_stat_common(f, statbuf);
    if (r) { vfs_close(f); return r; }

    // Close the file
    vfs_close(f);

    // Done
    return 0;
}
