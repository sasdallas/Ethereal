/**
 * @file hexahedron/task/syscalls/wait.c
 * @brief wait
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

long sys_wait(pid_t pid, int *wstatus, int options) {
    return process_waitpid(pid, wstatus, options);
}
