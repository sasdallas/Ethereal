/**
 * @file hexahedron/fs/console.c
 * @brief Framebuffer console driver (/device/console)
 * 
 * This provides a mini text device for shell mode.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/console.h>
#include <kernel/fs/vfs.h>
#include <stdio.h>
#include <string.h>
#include <kernel/gfx/video.h>
#include <kernel/gfx/term.h>
#include <kernel/debug.h>
#include <kernel/init.h>

/**
 * @brief Write method for console
 */
ssize_t console_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!size) return 0;

extern int video_ks;
    for (size_t i = 0; i < size; i++) {
        if (!video_ks) terminal_putchar(buffer[i]);
        else debug_print(NULL, buffer[i]);
    }

    return size;
}

/**
 * @brief Mount the console device
 */
static int console_mount() {
    fs_node_t *condev = fs_node();
    strncpy(condev->name, "console", 256);
    condev->flags = VFS_CHARDEVICE;
    condev->mask = 0777;
    condev->write = console_write;
    vfs_mount(condev, "/device/console");
    return 0;
}

FS_INIT_ROUTINE(console, INIT_FLAG_DEFAULT, console_mount);