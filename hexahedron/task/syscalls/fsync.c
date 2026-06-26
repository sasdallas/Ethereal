/**
 * @file hexahedron/task/syscalls/fsync.c
 * @brief fsync
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

long sys_fsync(int fd) {
    if (!FD_VALIDATE(fd)) {
        return -EBADF;
    }

    // fsync is stub
    return 0;
}
