/**
 * @file libpolyhedron/unistd/mkdir.c
 * @brief mkdir
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL2(mkdir, SYS_MKDIR, const char *, mode_t);

int mkdir(const char *pathname, mode_t mode) {
    __sets_errno(__syscall_mkdir(pathname, mode));
}