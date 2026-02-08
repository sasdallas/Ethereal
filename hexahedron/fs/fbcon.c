/**
 * @file hexahedron/fs/fbcon.c
 * @brief Provides /device/fbcon
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/devfs.h>
#include <kernel/init.h>
#include <kernel/gfx/term.h>
#include <kernel/debug.h>

static ssize_t fbcon_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer);

/* fbcon ops */
static devfs_ops_t fbcon_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = fbcon_write,
    .ioctl = NULL,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll_events = NULL,
    .poll = NULL,
};

/**
 * @brief fbcon write
 */
static ssize_t fbcon_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer) {
    if (!size) return 0;

extern int video_ks;
    for (size_t i = 0; i < size; i++) {
        if (!video_ks) terminal_putchar(buffer[i]);
        else debug_print(NULL, buffer[i]);
    }

    return size;
}

/**
 * @brief fbcon init
 */
static int fbcon_init() {
    return !devfs_register(devfs_root, "fbcon", VFS_BLOCKDEVICE, &fbcon_dev_ops, DEVFS_MAJOR_CONSOLE, 0, NULL);
}

FS_INIT_ROUTINE(fbcon, INIT_FLAG_DEFAULT, fbcon_init, devfs);