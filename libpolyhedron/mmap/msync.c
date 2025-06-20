/**
 * @file libpolyhedron/mmap/msync.c
 * @brief msync
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL3(msync, SYS_MSYNC, void*, size_t, int);

int msync(void *addr, size_t len, int flags) {
    __sets_errno(__syscall_msync(addr, len, flags));
}