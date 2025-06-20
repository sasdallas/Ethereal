/**
 * @file libpolyhedron/mmap/mprotect.c
 * @brief mprotect
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

DEFINE_SYSCALL3(mprotect, SYS_MPROTECT, void*, size_t, int);

int mprotect(void *addr, size_t len, int prot) {
    __sets_errno(__syscall_mprotect(addr, len, prot));
}