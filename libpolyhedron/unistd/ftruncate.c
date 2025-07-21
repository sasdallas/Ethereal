/**
 * @file libpolyhedron/unistd/ftruncate.c
 * @brief ftruncate
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

DEFINE_SYSCALL2(ftruncate, SYS_FTRUNCATE, int, off_t);

int ftruncate(int fd, off_t length) {
    __sets_errno(__syscall_ftruncate(fd, length));
}