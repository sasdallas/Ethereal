/**
 * @file libpolyhedron/unistd/lseek.c
 * @brief lseek
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <unistd.h>

DEFINE_SYSCALL3(lseek, SYS_LSEEK, int, off_t, int);

off_t lseek(int fd, off_t offset, int whence) {
    __sets_errno(__syscall_lseek(fd, offset, whence));
}