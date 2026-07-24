/**
 * @file hexahedron/task/syscalls/ftruncate.c
 * @brief ftruncate
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

long sys_ftruncate(int fd, size_t length) {
    if (!FD_VALIDATE(fd)) return -EBADF;
    return vfs_truncate(FD(fd)->inode, length); 
}
