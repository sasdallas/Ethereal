/**
 * @file libpolyhedron/unistd/pipe.c
 * @brief pipe function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL1(pipe, SYS_PIPE, int*);

int pipe(int fildes[2]) {
    __sets_errno(__syscall_pipe(fildes));
}