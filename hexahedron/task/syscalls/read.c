/**
 * @file hexahedron/task/syscalls/read.c
 * @brief read
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

ssize_t sys_read(int fd, void *buffer, size_t count) {
    SYSCALL_VALIDATE_PTR_SIZE(buffer, count);

    vfs_file_t *n = GET_FD_OR_ERROR(fd);

    ssize_t i = vfs_read(n, n->pos, count, (char*)buffer);
    if (i >= 0) n->pos += i;

    FD_FINISH(n);

    return i;
}
