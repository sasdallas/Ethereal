/**
 * @file hexahedron/task/syscalls/lseek.c
 * @brief lseek system call
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

off_t sys_lseek(int fd, off_t offset, int whence) {
    if (!FD_VALIDATE(fd)) {
        SYSCALL_LOG(ERR, "Bad file descriptor %d\n", fd);
        return -EBADF;
    }

    vfs_file_t *f = FD(fd);
    return vfs_seek(f, offset, whence);
}
