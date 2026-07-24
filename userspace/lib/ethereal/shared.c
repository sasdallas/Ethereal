/**
 * @file userspace/lib/ethereal/shared.c
 * @brief Ethereal shared memory API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/shared.h>
#include <sys/syscall.h>
#include <errno.h>

int shared_new(size_t size, int flags) {
    __sets_errno(SYSCALL2(SYS_SHARED_NEW, size, flags));
}

key_t shared_key(int fd) {
    __sets_errno(SYSCALL1(SYS_SHARED_KEY, fd));
}

int shared_open(key_t key) {
    __sets_errno(SYSCALL1(SYS_SHARED_OPEN, key));
}