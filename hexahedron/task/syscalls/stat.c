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

static int sys_stat_common(vfs_file_t *f, struct stat *statbuf) {
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

long sys_stat(const char *pathname, struct stat *statbuf) {
    vfs_file_t *f;
    int r = vfs_open((char*)pathname, O_NOFOLLOW, &f);
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
        SYSCALL_LOG(DEBUG, "lstat failed for %s error %d\n", pathname, r);
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
