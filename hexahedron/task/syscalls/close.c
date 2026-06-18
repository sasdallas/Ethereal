/**
 * @file hexahedron/task/syscalls/close.c
 * @brief close
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

int sys_close(int fd) {
    if (!FD_VALIDATE(fd)) {
        SYSCALL_LOG(WARN, "Bad file descriptor close attempt on fd %d\n", fd);
        return -EBADF;
    }

    return fd_remove(fd);
}
