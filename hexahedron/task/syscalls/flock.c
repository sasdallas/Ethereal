/**
 * @file hexahedron/task/syscalls/flock.c
 * @brief flock
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/syscall.h>

long sys_flock(int fd, int options) {
    SYSCALL_LOG(WARN, "Unimplemented (fd=%d options=%d)\n", fd, options);
    return 0;
}
