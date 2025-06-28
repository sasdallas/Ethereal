/**
 * @file libpolyhedron/termios/openpty.c
 * @brief openpty
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <termios.h>
#include <pty.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL5(openpty, SYS_OPENPTY, int*, int*, char *, const struct termios*, const struct winsize*);

int openpty(int *amaster, int *aslave, char *name, const struct termios *termp, const struct winsize *winp) {
    __sets_errno(__syscall_openpty(amaster, aslave, name, termp, winp));
}