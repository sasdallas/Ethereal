/**
 * @file hexahedron/task/syscalls/renameat.c
 * @brief renameat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/fs/vfs_new.h>
#include <unistd.h>

long sys_renameat(int olddirfd, const char *old_path, int newdirfd, const char *new_path, unsigned int flags) {
    SYSCALL_LOG(DEBUG, "sys_renameat\n");
    SYSCALL_VALIDATE_PTR(old_path);
    SYSCALL_VALIDATE_PTR(new_path);

    vfs_inode_t *src_inode = NULL;
    vfs_inode_t *dst_inode = NULL;

    if (*old_path == '/') {
        // Absolute path
    } else if (olddirfd == AT_FDCWD) {
        src_inode = current_cpu->current_process->wd_node;
    } else {
        if (!FD_VALIDATE(current_cpu->current_process, olddirfd)) return -EBADF;
        src_inode = FD(current_cpu->current_process, olddirfd)->node->inode;
    }
    if (src_inode) inode_hold(src_inode);

    // repeat for dest
    if (*new_path == '/') {
        // Absolute path
    } else if (newdirfd == AT_FDCWD) {
        dst_inode = current_cpu->current_process->wd_node;
    } else {
        if (!FD_VALIDATE(current_cpu->current_process, newdirfd)) {
            if (src_inode) inode_release(src_inode);
            return -EBADF;
        }
        
        dst_inode = FD(current_cpu->current_process, newdirfd)->node->inode;
    }
    if (dst_inode) inode_hold(dst_inode);

    int ret = vfs_renameat(src_inode, (char*)old_path, dst_inode, (char*)new_path, flags);
    if (src_inode) inode_release(src_inode);
    if (dst_inode) inode_release(dst_inode);
    return ret;
}