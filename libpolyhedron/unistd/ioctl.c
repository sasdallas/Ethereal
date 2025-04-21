/**
 * @file libpolyhedron/unistd/ioctl.c
 * @brief ioctl
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdarg.h>

DEFINE_SYSCALL3(ioctl, SYS_IOCTL, int, unsigned long, void*);

int ioctl(int fd, unsigned long request, ...) {
    // TODO: check this
    va_list ap;
    va_start(ap, request);
    void *argp = va_arg(ap, void*);
    va_end(ap);

    __sets_errno(__syscall_ioctl(fd, request, argp));
}