/**
 * @file hexahedron/task/syscalls/getcwd.c
 * @brief getcwd
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

long sys_getcwd(char *buffer, size_t size) {
    if (!size || !buffer) return 0;
    size_t wd_len = strlen(current_cpu->current_process->wd_path);
    strncpy(buffer, current_cpu->current_process->wd_path, (wd_len > size) ? size : wd_len);
    return 0;
}
