/**
 * @file libpolyhedron/unistd/uname.c
 * @brief uname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/utsname.h>
#include <unistd.h>

DEFINE_SYSCALL1(uname, SYS_UNAME, struct utsname *);

int uname(struct utsname *buf) {
    __sets_errno(__syscall_uname(buf));
}