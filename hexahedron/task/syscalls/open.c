/**
 * @file hexahedron/task/syscalls/open.c
 * @brief open system call
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
#include <kernel/debug.h>

#define UNSUPPORTED (O_NOCTTY | O_DSYNC | O_ASYNC | O_DIRECT | O_SYNC | O_RSYNC | O_LARGEFILE | O_NOATIME | O_TMPFILE)

int __sys_open_internal(char *pathname, int flags, mode_t mode) {
    if (flags & UNSUPPORTED) {
        SYSCALL_LOG(WARN, "open(%s, 0x%x, 0x%x) has unsupported flags\n", pathname, flags, mode);
    }

    // Try and get it open
    vfs_file_t *file;
    int r = vfs_open(pathname, flags, &file);

    // Did we find the node and they DIDN'T want us to create it?
    if (r == 0 && (flags & O_CREAT) && (flags & O_EXCL)) {
        vfs_close(file);
        return -EEXIST;
    }

    // Did we find it and did they want to create it?
    if ((r != 0) && (flags & O_CREAT)) {
        // Ok, make the file using some garbage hacks
        vfs_inode_t *ino_output;
        r = vfs_create(pathname, mode, &ino_output);
        if (r < 0) {
            return r;
        }

        // HACK: Open the node
        r = vfs_openat(ino_output, NULL, flags, &file);
        if (r != 0) {
            return r;
        }
    }

    // Did they want a directory?
    if ((r == 0) && !(file->inode->attr.type == VFS_DIRECTORY) && (flags & O_DIRECTORY)) {
        vfs_close(file);
        return -ENOTDIR;
    }

    // Did we find it and they want it?
    if (r) {
        return r;
    }

    // Truncate if needed
    if ((flags & O_TRUNC) && file->inode->attr.type == VFS_FILE) {
        r = vfs_truncate(file->inode, 0);
        if (r != 0) {
            vfs_close(file);
            return r;
        }
    }

    // Create the file descriptor and return
    int fd_out;
    r = fd_add(file, &fd_out);
    if (r < 0) {
        vfs_close(file);
        return r;
    }

    // !!!: very bad
    file->path = kmalloc(strlen(pathname) + strlen(current_cpu->current_process->wd_path) + 1);
    vfs_canonicalize(current_cpu->current_process->wd_path, pathname, file->path);

    // Are they trying to append? If so modify length to be equal to node length
    if (flags & O_APPEND) {
        r = vfs_seek(file, 0, SEEK_END);
        if (r < 0) return r;
    }

    return fd_out;
}

int sys_open(const char *pathname, int flags, mode_t mode) {
    SYSCALL_VALIDATE_PTR(pathname);
    int r = __sys_open_internal((char*)pathname, flags, mode);
    return r;
}

long sys_openat(int dirfd, const char *pathname, int flags, mode_t mode) {
    // SYSCALL_VALIDATE_PTR(pathname);

    // if (dirfd == AT_FDCWD || pathname[0] == '/') {
    //     // Easy enough
    //     return sys_open(pathname, flags, mode);
    // }

    // if (!FD_VALIDATE(dirfd) || FD(dirfd)->path == NULL) {
    //     return -EBADF;
    // }

    // fd_t *f = FD(dirfd);
    // if (!(f->node->inode->attr.type == VFS_DIRECTORY)) {
    //     return -ENOTDIR;
    // }

    // char *p = vfs_canonicalizePath(f->path, (char*)pathname);
    // long r = __sys_open_internal(p, flags, mode);
    // kfree(p);
    // return r;
    assert(0);
}
