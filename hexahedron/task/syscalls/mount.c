/**
 * @file hexahedron/task/syscalls/mount.c
 * @brief mount
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

long sys_mount(const char *src, const char *dst, const char *type, unsigned long flags, const void *data) {
    if (!type) {
        SYSCALL_LOG(ERR, "Lack of type is not supported\n");
        return -ENOTSUP;
    }

    // The current process must be root to mount
    if (current_cpu->current_process->uid != 0) {
        return -EPERM;
    }

    if (strlen(src) > PATH_MAX) return -ENAMETOOLONG;
    if (strlen(dst) > PATH_MAX) return -ENAMETOOLONG; 

    // get filesystem
    vfs2_filesystem_t *fs = vfs_getFilesystem((char*)type);
    if (!fs) { return -ENODEV; }

    // Canonicalize paths
    char *src_canonicalized = kmalloc(strlen(src) + strlen(current_cpu->current_process->wd_path) + 1);
    char *dst_canonicalized = kmalloc(strlen(src) + strlen(current_cpu->current_process->wd_path) + 1);  

    if (vfs_canonicalize(current_cpu->current_process->wd_path, (char*)src, src_canonicalized)) { kfree(src_canonicalized); kfree(dst_canonicalized); return -EINVAL; }
    if (vfs_canonicalize(current_cpu->current_process->wd_path, (char*)dst, dst_canonicalized)) { kfree(src_canonicalized); kfree(dst_canonicalized); return -EINVAL; }

    // Try to mount filesystem type
    int success = vfs2_mount(fs, src_canonicalized, (char*)dst_canonicalized, 0, NULL);
    kfree(src_canonicalized);
    kfree(dst_canonicalized);

    return success;
}
