/**
 * @file libpolyhedron/unistd/chown.c
 * @brief chown 
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
#include <sys/libc_debug.h>

int chown(const char *path, uid_t uid, gid_t group) {
    dprintf("libc: chown: %s (stub)\n", path);
    return 0;
}