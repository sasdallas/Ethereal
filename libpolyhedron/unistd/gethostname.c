/**
 * @file libpolyhedron/unistd/gethostname.c
 * @brief gethostname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

DEFINE_SYSCALL2(gethostname, SYS_GETHOSTNAME, char*, size_t);

int gethostname(char *name, size_t size) {
    __sets_errno(__syscall_gethostname(name, size));
}