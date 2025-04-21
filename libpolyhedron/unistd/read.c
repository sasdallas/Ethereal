/**
 * @file libpolyhedron/unistd/read.c
 * @brief read
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/syscall.h>
#include <unistd.h>

DEFINE_SYSCALL3(read, SYS_READ, int, void*, size_t);
 
ssize_t read(int fd, void *buffer, size_t count) {
    __sets_errno(__syscall_read(fd, buffer, count));
}