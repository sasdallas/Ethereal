/**
 * @file libpolyhedron/unistd/dup2.c
 * @brief dup2 system call handler
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

DEFINE_SYSCALL2(dup2, SYS_DUP2, int, int);

int dup2(int oldfd, int newfd) {
    __sets_errno(__syscall_dup2(oldfd, newfd));
}