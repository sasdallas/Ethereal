/**
 * @file hexahedron/fs/log.c
 * @brief Log device
 * 
 * The log device serves as a device for the userspace processes to attach to
 * and write to the initial kernel log properly
 * 
 * It tracks what processes log in to the log device, the time they sent messages, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/log.h>
#include <kernel/fs/periphfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/gfx/term.h>
#include <kernel/processor_data.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>

void log_open(fs_node_t*, unsigned int);
void log_close(fs_node_t*);
ssize_t logdev_write(fs_node_t *node, off_t off, size_t size, uint8_t *buf);

/* Log lock */
// spinlock_t log_lock = { 0 };
extern spinlock_t debug_lock;

/**
 * @brief Log device open method
 * @param node The node to open
 * @param flags The flags with which to open the node
 */
int logdev_open(fs_node_t *node, unsigned int flags) {
    char b[256];
    snprintf(b, 256, "Process %s connected to log daemon\n", current_cpu->current_process->name);
    node->write(node, 0, strlen(b), (uint8_t*)b);
    return 0;
}

/**
 * @brief Log device clse method
 * @param node The node to close
 */
int logdev_close(fs_node_t *node) {
    char b[256];
    snprintf(b, 256, "Process %s disconnected from log daemon\n", current_cpu->current_process->name);
    node->write(node, 0, strlen(b), (uint8_t*)b);
    return 0;
}

/**
 * @brief Log device print method
 */
int log_print(void *user, char ch) {
    return debug_print(user, ch);
}

/**
 * @brief Log device write method
 * @param node The node to write to
 * @param off The offset
 * @param size The size of the data to write
 * @param buffer The buffer to write
 */
ssize_t logdev_write(fs_node_t *node, off_t off, size_t size, uint8_t *buf) {
    if (!size) return 0;

    if (debug_lock.cpu != arch_current_cpu()) {
        // !!!: This is bad. Should use a mutex here..
        spinlock_acquire(&debug_lock);
    }

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);

    char header[256];
    snprintf(header, 256, "[%s] [PROC] [%s:%d] ", asctime(timeinfo), current_cpu->current_process->name, current_cpu->current_process->pid);
    for (size_t i = 0; i < strlen(header); i++) log_print(NULL, header[i]);
    for (size_t i = 0; i < size; i++) log_print(NULL, buf[i]);

    spinlock_release(&debug_lock);
    return size;
}



/**
 * @brief Mount log device
 */
void log_mount() {
    fs_node_t *log_node = kzalloc(sizeof(fs_node_t));
    strncpy(log_node->name, "log", 256);
    log_node->flags = VFS_CHARDEVICE;
    log_node->atime = log_node->ctime = log_node->mtime = now();
    log_node->gid = log_node->uid = 0;
    log_node->open = logdev_open;
    log_node->close = logdev_close;
    log_node->write = logdev_write;
    log_node->mask = 0001;

    vfs_mount(log_node, "/device/log");
}