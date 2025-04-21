/**
 * @file libpolyhedron/unistd/stat.c
 * @brief stat, fstat, and lstat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

DEFINE_SYSCALL2(stat, SYS_STAT, const char*, struct stat*);
DEFINE_SYSCALL2(fstat, SYS_FSTAT, int, struct stat*);
DEFINE_SYSCALL2(lstat, SYS_LSTAT, const char*, struct stat*);

int stat(const char *pathname, struct stat *statbuf) {
    __sets_errno(__syscall_stat(pathname, statbuf));
}

int fstat(int fd, struct stat *statbuf) {
    __sets_errno(__syscall_fstat(fd, statbuf));
}

int lstat(const char *pathname, struct stat *statbuf) {
    __sets_errno(__syscall_lstat(pathname, statbuf));
}