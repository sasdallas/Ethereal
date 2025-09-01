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
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <sys/ioctl_ethereal.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

void log_open(fs_node_t*, unsigned int);
void log_close(fs_node_t*);
ssize_t logdev_write(fs_node_t *node, off_t off, size_t size, uint8_t *buf);

/* Log lock */
// spinlock_t log_lock = { 0 };
ssize_t debug_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);

typedef struct logfs_buffer {
    char *buffer;
    size_t size;
    size_t index;
    spinlock_t lck;
} logfs_buffer_t;

/**
 * @brief Log device open method
 * @param node The node to open
 * @param flags The flags with which to open the node
 */
int logdev_open(fs_node_t *node, unsigned int flags) {
    node->dev = kzalloc(sizeof(logfs_buffer_t));
    logfs_buffer_t *buf = (logfs_buffer_t*)node->dev;

    buf->buffer = kzalloc(1024);
    buf->size = 1024;
    buf->index = 0;
    return 0;  
}

/**
 * @brief Log device clse method
 * @param node The node to close
 */
int logdev_close(fs_node_t *node) {
    logfs_buffer_t *buf = (logfs_buffer_t*)node->dev;
    kfree(buf->buffer);
    kfree(buf);
    return 0;
}

/**
 * @brief Log device ioctl method for emulating TTY
 */
int logdev_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    if (request == IOCTLTTYIS) {
        SYSCALL_VALIDATE_PTR(argp);
        *(int*)argp = 1;
        return 0;
    }

    return -EINVAL;
}

/**
 * @brief Log device print method
 */
int log_print(void *user, char ch) {
    return debug_print(user, ch);
}

/**
 * @brief Flush method
 */
void log_flush(logfs_buffer_t *buf) {
    
    // Determine kernel boot time
    unsigned long seconds, subseconds;
    clock_relative(0, 0, &seconds, &subseconds);

    char header[256];
    snprintf(header, 256, "[%lu.%06lu] [PROC] [%s:%d] ", seconds, subseconds, current_cpu->current_process->name, current_cpu->current_process->pid);
    debug_write(NULL, 0, strlen(header), (uint8_t*)header);
    debug_write(NULL, 0, buf->index, (uint8_t*)buf->buffer);

    buf->index = 0;
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

    // Push into buffer
    logfs_buffer_t *b = (logfs_buffer_t*)node->dev;
    spinlock_acquire(&b->lck);
    for (unsigned i = 0; i < size; i++) {
        b->buffer[b->index++] = buf[i];
        if (b->index >= b->size || buf[i] == '\n') log_flush(b);
        b->buffer[b->index] = 0;
    }
    spinlock_release(&b->lck);

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
    log_node->ioctl = logdev_ioctl;
    log_node->mask = 0600;

    vfs_mount(log_node, "/device/log");
}