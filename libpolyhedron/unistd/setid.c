/**
 * @file libpolyhedron/unistd/setid.c
 * @brief setid functions
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
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL1(setuid, SYS_SETUID, uid_t);
DEFINE_SYSCALL1(setgid, SYS_SETGID, gid_t);
DEFINE_SYSCALL0(setsid, SYS_SETSID);
DEFINE_SYSCALL1(seteuid, SYS_SETEUID, uid_t);
DEFINE_SYSCALL1(setegid, SYS_SETEGID, gid_t);
DEFINE_SYSCALL2(setpgid, SYS_SETPGID, pid_t, pid_t);

int setuid(uid_t uid) {
    __sets_errno(__syscall_setuid(uid));
}

int setgid(gid_t gid) {
    __sets_errno(__syscall_setgid(gid));
}

pid_t setsid() {
    __sets_errno(__syscall_setsid());
}

int seteuid(uid_t uid) {
    __sets_errno(__syscall_seteuid(uid));
}

int setegid(gid_t gid) {
    __sets_errno(__syscall_setegid(gid));
}

int setpgid(pid_t pid, pid_t pgid) {
    __sets_errno(__syscall_setpgid(pid, pgid));
}

pid_t setpgrp() {
    return setpgid(0, 0);
}