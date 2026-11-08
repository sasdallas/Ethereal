/**
 * @file hexahedron/task/syscalls/mknodat.c
 * @brief mknodat
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
#include <sys/stat.h>

long sys_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev) {
    SYSCALL_LOG(ERR, "unimplemented\n");
    return -ENOSYS;
}
