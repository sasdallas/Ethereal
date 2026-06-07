/**
 * @file hexahedron/task/syscalls/chdir.c
 * @brief chdir
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

long sys_chdir(const char *path) {
    SYSCALL_VALIDATE_PTR(path);

    // !!!: this is bad and not safe
    vfs_inode_t *i;
    int res = vfs_lookup((char*)path, &i, LOOKUP_DEFAULT);
    if (res == 0) {
        if (i->attr.type != VFS_DIRECTORY) {
            inode_release(i);
            return -ENOTDIR;
        }

        char tmp[strlen(current_cpu->current_process->wd_path) + strlen(path) + 1];
        vfs_canonicalize(current_cpu->current_process->wd_path, (char*)path, tmp);

        kfree(current_cpu->current_process->wd_path);
        current_cpu->current_process->wd_path = strdup(tmp);
        
        vfs_inode_t *old = current_cpu->current_process->wd_node;
        current_cpu->current_process->wd_node = i; // already locked
        inode_release(old);

        return 0;
    }

    return res;
}

long sys_fchdir(int fd) {
    vfs_file_t *f = GET_FD_OR_ERROR(fd);
    if (f->inode->attr.type != VFS_DIRECTORY) {
        SYSCALL_LOG(DEBUG, "what dumbass just chdir'd to %s (%d), which is %d?\n", f->path, fd, f->inode->attr.type); 
        FD_FINISH(f);
        return -ENOTDIR;
    }

    // this uses an annoying hack...
    // !!!: also this is not safe
    char *oldpath = current_cpu->current_process->wd_path;
    current_cpu->current_process->wd_path = strdup(f->path);
    if (oldpath) kfree(oldpath);

    vfs_inode_t *old = current_cpu->current_process->wd_node;
    current_cpu->current_process->wd_node = f->inode; // already locked
    inode_release(old);

    return 0;
}
