/**
 * @file hexahedron/task/syscalls/read_entries.c
 * @brief read_entries
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

long sys_read_entries(int handle, void *buffer, size_t max_size) {
    if (!FD_VALIDATE(handle)) return -EBADF;

    vfs_file_t *f = FD(handle);

    vfs_dir_context_t ctx = {
        .dirpos = f->pos
    };

    unsigned char *p = (unsigned char*)buffer;
    size_t read = 0;
    while (read + sizeof(struct dirent) <= max_size) {
        int r = file_get_entries(f, &ctx);
        if (r == 1) {
            break;
        }

        if (r != 0) {
            return r;
        }

        ctx.dirpos++;
        f->pos++;

        struct dirent *ent = (struct dirent*)p;
        strncpy(ent->d_name, ctx.name, 256);
        
        ent->d_ino = ctx.ino;
        ent->d_type = ctx.type;
        ent->d_reclen = sizeof(struct dirent);

        p += sizeof(struct dirent);
        read += sizeof(struct dirent);
    }  

    return read;
}
