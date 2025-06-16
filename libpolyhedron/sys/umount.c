/**
 * @file libpolyhedron/sys/umount.c
 * @brief umount
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

DEFINE_SYSCALL1(umount, SYS_UMOUNT, const char *);

int umount(const char *target) {
    __sets_errno(__syscall_umount(target));
}