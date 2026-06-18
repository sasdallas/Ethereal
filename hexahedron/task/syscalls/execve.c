/**
 * @file hexahedron/task/syscalls/execve.c
 * @brief execve
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
#include <kernel/loader/binfmt.h>

long sys_execve(const char *pathname, const char *argv[], const char *envp[]) {
    vfs_file_t *f;
    int r = vfs_open((char*)pathname, O_RDONLY, &f);
    if (r) return r;
    if ((f->inode->attr.type == VFS_DIRECTORY)) { vfs_close(f); return -EISDIR; }

    // Collect any arguments that we need
    int argc = 0;
    int envc = 0;

    // Collect argc
    while (argv[argc]) {
        argc++;
    }

    // Collect envc
    if (envp) {
        while (envp[envc]) {
            envc++;
        }
    }

    // Move their arguments into our array
    char **new_argv = kzalloc((argc+1) * sizeof(char*));
    for (int a = 0; a < argc; a++) {
        new_argv[a] = strdup(argv[a]);
    }

    // Reallocate envp if specified
    char **new_envp = kzalloc((envc+1) * sizeof(char*));
    if (envp) {
        for (int e = 0; e < envc; e++) {
            new_envp[e] = strdup(envp[e]);
        }
    } 

    // Set null terminators
    new_argv[argc] = NULL;
    new_envp[envc] = NULL;

    return binfmt_exec((char*)pathname, f, argc, new_argv, new_envp);
}
