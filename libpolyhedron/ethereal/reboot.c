/**
 * @file libpolyhedron/ethereal/reboot.c
 * @brief Ethereal reboot API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/ethereal/reboot.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL1(reboot, SYS_REBOOT, int);

int ethereal_reboot(int operation) {
    __sets_errno(__syscall_reboot(operation));
}