/**
 * @file hexahedron/task/syscalls/pwrite.c
 * @brief pwrite
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
#include <kernel/fs/vfs.h>

long sys_pwrite(int fd, void *buf, size_t nbyte, off_t offset) {
    vfs_file_t *f = GET_FD_OR_ERROR(fd);
    ssize_t ret = vfs_write(f, offset, nbyte, buf);
    FD_FINISH(f);
    return ret;
}
