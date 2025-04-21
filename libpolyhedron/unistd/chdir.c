/**
 * @file libpolyhedron/unistd/chdir.c
 * @brief chdir
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>

DEFINE_SYSCALL1(chdir, SYS_CHDIR, const char*)
DEFINE_SYSCALL1(fchdir, SYS_FCHDIR, int);

int chdir(const char *path) {
    __sets_errno(__syscall_chdir(path));
}

int fchdir(int fd) {
    __sets_errno(__syscall_fchdir(fd));
}