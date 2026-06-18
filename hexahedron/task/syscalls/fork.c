/**
 * @file hexahedron/task/syscalls/fork.c
 * @brief fork
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

pid_t sys_fork() {
    pid_t cpid = process_fork();
    return cpid;
}
