/**
 * @file hexahedron/fs/initrd.c
 * @brief Initial ramdisk unpacker
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/initrd.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/debug.h>
#include <string.h>
#include <stdlib.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:INITRD", __VA_ARGS__)

/* Round to 512 */
#define USTAR_SIZE(size) ((size % 512) ? (size + (512 - (size % 512))) : size)

/**
 * @brief Unpack the initial ramdisk to /
 * @param buffer A pointer to the ramdisk buffer in memory
 * @param size The size of the ramdisk buffer in memory
 * @returns 0 on success
 */
int initrd_unpack(uint8_t *buffer, size_t size) {
    LOG(DEBUG, "Unpacking initial ramdisk...\n");

    int entries_unpacked = 0;

    // Get the starting time
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    // Begin with the root inode
    ustar_header_t *h = (ustar_header_t*)buffer;

    if (memcmp(h->ustar, "ustar", 5)) {
        LOG(ERR, "Not a USTAR archive, unpack failed.\n");
        return 1;
    }

    while (!memcmp(h->ustar, "ustar", 5)) {
        // Get the full path + file size
        size_t nl = strlen(h->name) + strlen(h->nameprefix) + 3;
        char full_path[nl];
        snprintf(full_path, nl, "/%s/%s", h->nameprefix, h->name);

        size_t file_size = strtoull(h->size, NULL, 8);

        if (!strncmp(full_path, "///", nl)) goto _next_entry; //hack 
        LOG(DEBUG, "Unpacking: %s\n", full_path);

        if (h->type[0] == USTAR_FILE) {
            // Regular file
            mode_t m = strtol(h->mode, NULL, 8);

            vfs_inode_t *i_res;
            int r = vfs_create(full_path, m, &i_res);
            if (r) {
                LOG(ERR, "Failed to create file %s (error code %d)\n", full_path, r);
                goto _next_entry;
            }

            // Now update the inode's attributes and flush it
            i_res->attr.atime = i_res->attr.mtime = i_res->attr.ctime = strtol(h->mtime, NULL, 8);
            i_res->attr.gid = strtol(h->gid, NULL, 8);
            i_res->attr.uid = strtol(h->uid, NULL, 8);
            flush_inode(i_res); // write back to disk

            // Open up the inode
            vfs_file_t *f;
            r = vfs_openat(i_res, NULL, O_RDWR, &f);
            if (r) {
                inode_release(i_res);
                LOG(ERR, "Failed to open file %s (error code %d)\n", full_path, r);
                goto _next_entry;
            }


            // Write
            r = vfs_write(f, 0, file_size, (const char*)((uint8_t*)h + 512));
            if (r != (int)file_size) {
                vfs_close(f);
                inode_release(i_res);
                LOG(ERR, "Failed to write file %s (error code %d)\n", full_path, r);
                goto _next_entry;
            }

            // release
            vfs_close(f);
            inode_release(i_res);
        } else if (h->type[0] == USTAR_DIRECTORY) {
            // Directory
            mode_t m = strtol(h->mode, NULL, 8);

            int r = vfs_mkdirat(NULL, full_path, m, NULL);
            if (r) {
                LOG(ERR, "Failed to create directory %s (error code %d)\n", full_path, r);
                goto _next_entry;
            }

            // TODO update dir attributes
        } else if (h->type[0] == USTAR_HARD_LINK) {
            
        } else if (h->type[0] == USTAR_SYMLINK) {
            // unpack symlink
            int r = vfs2_symlink(h->linkname, full_path, NULL);
            if (r) {
                LOG(ERR, "Failed to create symlink %s -> %s (error code %d)\n", full_path, h->linkname, r);
                goto _next_entry;
            }
        } else if (h->type[0] == USTAR_CHARDEV) {

        } else if (h->type[0] == USTAR_BLOCKDEV) {

        } else if (h->type[0] == USTAR_PIPE) {

        } else {
            LOG(WARN, "Unrecognized USTAR file type: %c\n", h->type[0]);
        }

        entries_unpacked++;

    _next_entry:
        h = (ustar_header_t*)((uint8_t*)h + 512 + USTAR_SIZE(file_size));
    }

    // Get the ending time
    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);
    
    // taken from mlibc
    struct timeval tv_sub;
    tv_sub.tv_sec = tv_end.tv_sec - tv_start.tv_sec;
    tv_sub.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
    while(tv_sub.tv_usec < 0) {
		tv_sub.tv_usec += 1000000;
		tv_sub.tv_sec -= 1;
	}

    LOG(DEBUG, "Unpacked %d entries in %d.%ds\n", entries_unpacked, tv_sub.tv_sec, tv_sub.tv_usec);
    return 0;
}