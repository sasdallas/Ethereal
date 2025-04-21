/**
 * @file libpolyhedron/unistd/close.c
 * @brief close
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
#include <errno.h>

DEFINE_SYSCALL1(close, SYS_CLOSE, int);

int close(int fd) {
    __sets_errno(__syscall_close(fd));
}