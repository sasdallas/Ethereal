/**
 * @file hexahedron/task/syscalls/umask.c
 * @brief umask
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

mode_t sys_umask(mode_t mask) {
    process_t *this = current_cpu->current_process;
    mode_t last = this->umask;
    this->umask = mask & 0777;
    return last;
}
