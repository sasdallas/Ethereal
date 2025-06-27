/**
 * @file libpolyhedron/unistd/open.c
 * @brief open syscall
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
#include <stdarg.h>
#include <fcntl.h>

DEFINE_SYSCALL3(open, SYS_OPEN, const char*, int, mode_t);

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;

    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT) mode = va_arg(ap, mode_t); // TODO: Also check for O_TMPFILE. This statement could fail!
    va_end(ap);

    __sets_errno(__syscall_open(pathname, flags, mode));
}