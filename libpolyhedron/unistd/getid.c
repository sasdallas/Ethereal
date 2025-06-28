/**
 * @file libpolyhedron/unistd/getid.c
 * @brief getid functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

DEFINE_SYSCALL0(getuid, SYS_GETUID);
DEFINE_SYSCALL0(getgid, SYS_GETGID);
DEFINE_SYSCALL0(getppid, SYS_GETPPID);
DEFINE_SYSCALL0(getsid, SYS_GETSID);
DEFINE_SYSCALL0(geteuid, SYS_GETEUID);
DEFINE_SYSCALL0(getegid, SYS_GETEGID);
DEFINE_SYSCALL1(getpgid, SYS_GETPGID, pid_t);

uid_t getuid() {
    __sets_errno(__syscall_getuid());
}

gid_t getgid() {
    __sets_errno(__syscall_getgid());
}

pid_t getppid() {
    __sets_errno(__syscall_getppid());
}

pid_t getsid() {
    __sets_errno(__syscall_getsid());
}

uid_t geteuid() {
    __sets_errno(__syscall_geteuid());
}

gid_t getegid() {
    __sets_errno(__syscall_getegid());
}

pid_t getpgid(pid_t pid) {
    __sets_errno(__syscall_getpgid(pid));
}

pid_t getpgrp() {
    return getpgid(0);
}