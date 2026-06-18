/**
 * @file hexahedron/task/syscalls/openpty.c
 * @brief openpty
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/fs/tty.h>

long sys_openpty(int *amaster, int *aslave, char *name, const struct termios *termp, const struct winsize *winp) {
    if (termp) SYSCALL_VALIDATE_PTR(termp);
    if (winp) SYSCALL_VALIDATE_PTR(winp);
    SYSCALL_VALIDATE_PTR(amaster);
    SYSCALL_VALIDATE_PTR(aslave);

    // Make a PTY
    vfs_file_t *master;
    vfs_file_t *slave;
    pty_t *pty;
    int r = pty_create(&pty, &master, &slave);
    if (r) return r;

    if (name) {
        SYSCALL_VALIDATE_PTR(name);
        strcpy(name, pty->slave->name);
    }

    r = fd_add(master, amaster);
    if (r < 0) assert(0); // todo
    r = fd_add(slave, aslave);
    if (r < 0) assert(0); // todo

    return 0;
}
