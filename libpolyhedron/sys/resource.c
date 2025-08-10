/**
 * @file libpolyhedron/sys/resource.c
 * @brief Resource stubs
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/resource.h>
#include <errno.h>
#include <stdio.h>

int getrlimit(int resource, struct rlimit *rlp) {
    fprintf(stderr, "libc: getrlimit: stub\n");
    errno = ENOSYS;
    return -1;
}

int getrusage(int who, struct rusage *r_usage) {
    r_usage->ru_utime.tv_sec = 0;
    r_usage->ru_utime.tv_usec = 0;
    r_usage->ru_stime.tv_sec = 0;
    r_usage->ru_stime.tv_usec = 0;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlp) {
    fprintf(stderr, "libc: setrlimit: stub\n");
    errno = ENOSYS;
    return -1;
}