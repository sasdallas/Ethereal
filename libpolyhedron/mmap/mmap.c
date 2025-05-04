/**
 * @file libpolyhedron/mmap/mmap.c
 * @brief mmap
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

/* Used as kernel has no support for 6-parameter system calls */
struct __mmap_context {
    void *addr;
    size_t len;
    int prot;
    int flags;
    int filedes;
    off_t off;
};

/* mmap syscall */
DEFINE_SYSCALL1(mmap, SYS_MMAP, void*);

void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off) {
    struct __mmap_context context = {
        .addr = addr,
        .len = len,
        .prot = prot,
        .flags = flags,
        .filedes = fildes,
        .off = off
    };

    long ret = __syscall_mmap(&context);
    if ((int)ret < 0) {
        // Error
        errno = -ret;
        return MAP_FAILED;
    }

    return (void*)ret;
}