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
 * @brief Keyboard device read
 */
static ssize_t keyboard_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;
    
    // This will cause havoc if allowed
    if (size % sizeof(key_event_t)) {
        LOG(WARN, "Read from /device/keyboard denied - size must be multiple of key_event_t\n");
        return 0;
    }

    circbuf_t *buf = (circbuf_t*)node->dev;

    // TODO: This is really really bad.. like actually horrendous. We should also be putting the thread to sleep
    while (circbuf_read(buf, size, buffer)) {
        while (buf->head == buf->tail) arch_pause();
    }

    
    return size;
}

/**
 * @brief Generic stdin device read
 */
static ssize_t stdin_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;

    circbuf_t *buf = (circbuf_t*)node->dev;

    // Start reading key events
    key_event_t event;
    
    for (size_t i = 0; i < size; i++) {
        while (1) {
            // TODO: This is really really bad.. like actually horrendous. We should also be putting the thread to sleep
            while (circbuf_read(buf, sizeof(key_event_t), (uint8_t*)&event)) {
                // LOG(DEBUG, "CWAIT\n");
                while (buf->head == buf->tail) arch_pause();
            }

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
    circbuf_t *buf = (circbuf_t*)node->dev;
    return !(buf->head == buf->tail);
}

/**
 * @brief Initialize the peripheral filesystem interface
 */
void periphfs_init() {
    // Create keyboard circular buffer
    circbuf_t *kbd_buffer = circbuf_create("kbd buffer", sizeof(key_event_t) * 512);

    // Create and mount keyboard node
    kbd_node = kmalloc(sizeof(fs_node_t));
    memset(kbd_node, 0, sizeof(fs_node_t));
    strcpy(kbd_node->name, "keyboard");
    kbd_node->flags = VFS_CHARDEVICE;
    kbd_node->dev = (void*)kbd_buffer;
    kbd_node->read = keyboard_read;
    kbd_node->ioctl = periph_ioctl;
    vfs_mount(kbd_node, "/device/keyboard");

    // Create and mount keyboard node
    stdin_node = kmalloc(sizeof(fs_node_t));
    memset(stdin_node, 0, sizeof(fs_node_t));
    strcpy(stdin_node->name, "stdin");
    stdin_node->flags = VFS_CHARDEVICE;
    stdin_node->dev = (void*)kbd_buffer;
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


    circbuf_write((circbuf_t*)kbd_node->dev, sizeof(key_event_t), (uint8_t*)&event);
    LOG(DEBUG, "SEND key event type=%d\n", event_type);
    return 0;
}
