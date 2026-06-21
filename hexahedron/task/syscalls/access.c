/**
 * @file hexahedron/task/syscalls/access.c
 * @brief access
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

long sys_access(const char *path, int amode) {
    SYSCALL_VALIDATE_PTR(path);

    int flags = 0;

    // TODO: F_OK and X_OK
    if (amode & R_OK) flags |= O_RDONLY;
    if (amode & W_OK) flags |= O_WRONLY;

    vfs_file_t *f;
    int r = vfs_open((char*)path, flags, &f);
    if (r < 0) return r;
    vfs_close(f);
    return 0;
}
