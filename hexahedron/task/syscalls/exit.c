/**
 * @file hexahedron/task/syscalls/exit.c
 * @brief exit
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>

void sys_exit(int status) {
    syscall_finish();
    process_exit(NULL, status);
}
