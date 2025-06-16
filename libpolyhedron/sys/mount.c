/**
 * @file libpolyhedron/sys/mount.c
 * @brief mount system call
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/mount.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL5(mount, SYS_MOUNT, const char*, const char*, const char*, unsigned long, const void*);

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
    __sets_errno(__syscall_mount(source, target, filesystemtype, mountflags, data));
}