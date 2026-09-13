/**
 * @file hexahedron/task/syscalls/dup2.c
 * @brief dup2
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

long sys_dup2(int oldfd, int newfd) {
    if (!FD_VALIDATE(oldfd)) {
        return -EBADF;
    }

    int fd_out;
    int err = fd_duplicate(oldfd, newfd, &fd_out, true);
    if (err != 0) return err;
    return fd_out;
}
