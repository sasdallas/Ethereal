/**
 * @file hexahedron/task/syscalls/ioctl.c
 * @brief ioctl
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

long sys_ioctl(int fd, unsigned long request, void *argp) {
    if (!FD_VALIDATE(fd)) return -EBADF;
    return vfs_ioctl(FD(fd), request, argp);
}
