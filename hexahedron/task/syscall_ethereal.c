/**
 * @file hexahedron/task/syscall_ethereal.c
 * @brief Ethereal specific system calls
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/syscall.h>
#include <kernel/task/fd.h>
#include <kernel/fs/shared.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL:ETHEREAL", __VA_ARGS__)

/**** SHARED MEMORY API ****/

long sys_ethereal_shared_new(size_t size, int flags) {
    return sharedfs_new(current_cpu->current_process, size, flags);
}

key_t sys_ethereal_shared_key(int fd) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) return -EBADF;
    return sharedfs_key(FD(current_cpu->current_process, fd)->node);
}

long sys_ethereal_shared_open(key_t key) {
    return sharedfs_openFromKey(current_cpu->current_process, key);
}