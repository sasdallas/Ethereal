/**
 * @file libpolyhedron/unistd/write.c
 * @brief write
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/syscall.h>
#include <unistd.h>

DEFINE_SYSCALL3(write, SYS_WRITE, int, const void*, size_t);

ssize_t write(int fd, const void *buffer, size_t count) {
    __sets_errno(__syscall_write(fd, buffer, count));
}