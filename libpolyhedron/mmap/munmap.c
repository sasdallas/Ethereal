/**
 * @file libpolyhedron/mmap/munmap.c
 * @brief munmap
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>

DEFINE_SYSCALL2(munmap, SYS_MUNMAP, void*, size_t);

int munmap(void *addr, size_t len) {
    __sets_errno(__syscall_munmap(addr, len));
}