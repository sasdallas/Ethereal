/**
 * @file libpolyhedron/unistd/getcwd.c
 * @brief getcwd
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

DEFINE_SYSCALL2(getcwd, SYS_GETCWD, char*, size_t);

char *getcwd(char *buf, size_t size) {
    // Extension to POSIX.1-2001 by glibd
    if (!buf && size) {
        buf = malloc(size);
        memset(buf, 0, size);
    }

    int result = __syscall_getcwd(buf, size);
    if (!result) return NULL;
    return buf;
}