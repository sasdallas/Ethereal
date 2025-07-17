/**
 * @file libpolyhedron/unistd/access.c
 * @brief access
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

DEFINE_SYSCALL2(access, SYS_ACCESS, const char*, int);

int access(const char *path, int amode) {
    __sets_errno(__syscall_access(path, amode));
}