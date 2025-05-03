/**
 * @file hexahedron/fs/periphfs.c
 * @brief Peripheral filesystem (keyboard + mouse)
 * 
 * Translation to scancodes is a driver-side task. The driver will build a packet and then
 * pass it to @c periphfs_packet() with the corresponding packet type. 
 * 
 * The peripheral system creates 3 mounts:
 * - /device/keyboard for receiving @c key_event_t structures
 * - /device/mouse for receiving @c mouse_event_t structures
 * - /device/input for receiving raw characters processed by peripheral filesystem. 
 * Note that reading from /device/stdin will also discard the corresponding key event.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/arch.h>
#include <kernel/fs/periphfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>
#include <structs/circbuf.h>
#include <string.h>

/* Filesystem nodes */
fs_node_t *kbd_node = NULL;
fs_node_t *mouse_node = NULL;
fs_node_t *stdin_node = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:PERIPHFS", __VA_ARGS__)

/**
 * @brief Get the last keyboard event (pop from the buffer)
 * @param event Event pointer
 * @returns 0 on success, 1 on failure
 */
static int periphfs_getKeyboardEvent(key_event_t *event) {
    // Get buffer
    key_buffer_t *buf = (key_buffer_t*)kbd_node->dev;

    // Get the lock
    spinlock_acquire(&buf->lock);
    
    // Increase and reset head
    buf->head++;
    if (buf->head > KBD_QUEUE_EVENTS) buf->head = 0;

    // Get event
    *event = buf->event[buf->head];

    // Release the lock
    spinlock_release(&buf->lock);

    return 0;
} 


/**
 * @brief Keyboard device read
 */
static ssize_t keyboard_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;
    
    // This will cause havoc if allowed
    if (size % sizeof(key_event_t)) {
        LOG(WARN, "Read from /device/keyboard denied - size must be multiple of key_event_t\n");
        return 0;
    }

    // Get buffer
    key_buffer_t *buf = (key_buffer_t*)node->dev;

    // TODO: This is really really bad.. like actually horrendous. We should also be putting the thread to sleep
    for (size_t i = 0; i < size; i += sizeof(key_event_t)) {
        if (!KEY_CONTENT_AVAILABLE(buf)) return i;
        periphfs_getKeyboardEvent((key_event_t*)(buffer + i));
    }

    
    return size;
}

/**
 * @brief Generic stdin device read
 */
static ssize_t stdin_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;

    key_buffer_t *buf = (key_buffer_t*)node->dev;

    // Start reading key events
    key_event_t event;
    
    for (size_t i = 0; i < size; i++) {
        while (1) {
            // Wait for content
            while (!KEY_CONTENT_AVAILABLE(buf)) arch_pause(); // !!!
            periphfs_getKeyboardEvent(&event);

            // Did we get a key press event?
            if (event.event_type != EVENT_KEY_PRESS) continue;
            *buffer = event.scancode; // !!!: What if scancode is for a special key?
            buffer++;
            break;
        }
    }

    return size;

}

/**
 * @brief ioctl
 */
int periph_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    int r = KEY_CONTENT_AVAILABLE((key_buffer_t*)node->dev);
    if (r) LOG(DEBUG, "Reporting that key content is available\n");
    else LOG(DEBUG, "No key content is available\n");
    return r;
}

/**
 * @brief Initialize the peripheral filesystem interface
 */
void periphfs_init() {
    // Create keyboard circular buffer
    key_buffer_t *kbdbuf = kmalloc(sizeof(key_buffer_t));
    memset(kbdbuf, 0, sizeof(key_buffer_t));

    // Create and mount keyboard node
    kbd_node = kmalloc(sizeof(fs_node_t));
    memset(kbd_node, 0, sizeof(fs_node_t));
    strcpy(kbd_node->name, "keyboard");
    kbd_node->flags = VFS_CHARDEVICE;
    kbd_node->dev = (void*)kbdbuf;
    kbd_node->read = keyboard_read;
    kbd_node->ioctl = periph_ioctl;
    vfs_mount(kbd_node, "/device/keyboard");

    // Create and mount keyboard node
    stdin_node = kmalloc(sizeof(fs_node_t));
    memset(stdin_node, 0, sizeof(fs_node_t));
    strcpy(stdin_node->name, "stdin");
    stdin_node->flags = VFS_CHARDEVICE;
    stdin_node->dev = (void*)kbdbuf;
    stdin_node->read = stdin_read;
    stdin_node->ioctl = periph_ioctl;
    vfs_mount(stdin_node, "/device/stdin");
}

/**
 * @brief Write a new event to the keyboard interface
 * @param event_type The type of event to write
 * @param scancode The scancode relating to the event
 * @returns 0 on success
 */
int periphfs_sendKeyboardEvent(int event_type, uint8_t scancode) {
    key_event_t event = {
        .event_type = event_type,
        .scancode = scancode
    };

    // Push!
    key_buffer_t *buffer = (key_buffer_t*)kbd_node->dev;

    // Reset tail if needed
    buffer->tail++;
    if (buffer->tail > KBD_QUEUE_EVENTS) buffer->tail = 0;

    // Set event
    buffer->event[buffer->tail] = event;

    LOG(DEBUG, "SEND key event type=%d\n", event_type);
    return 0;
}
