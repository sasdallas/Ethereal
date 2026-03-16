/**
 * @file hexahedron/fs/periphfs.c
 * @brief Gen2 peripheral filesystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/periphfs.h>
#include <kernel/fs/devfs.h>
#include <kernel/debug.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/init.h>
#include <string.h>
#include <structs/circbuf.h>

static ssize_t keyboard_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static int keyboard_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t keyboard_poll_events(devfs_node_t *n);
static poll_events_t keyboard_events();

static ssize_t mouse_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static int mouse_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t mouse_events();

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:PERIPHFS", __VA_ARGS__)

static devfs_ops_t kbd_ops = {
    .open = NULL,
    .close = NULL,
    .read = keyboard_read,
    .write = NULL,
    .ioctl = NULL, // maybe
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll = keyboard_poll,
    .poll_events = (typeof(kbd_ops.poll_events))keyboard_events,
};

static devfs_ops_t mouse_ops = {
    .open = NULL,
    .close = NULL,
    .read = mouse_read,
    .write = NULL,
    .ioctl = NULL, // maybe
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll = mouse_poll,
    .poll_events = (typeof(mouse_ops.poll_events))mouse_events,
};

/* Circular buffers */
circbuf_t *kbd_buffer = NULL;
circbuf_t *mouse_buffer = NULL;

/* Keyboard poll event */
poll_event_t kbd_poll_event;
poll_event_t mouse_poll_event;

/**
 * @brief Keyboard read
 */
static ssize_t keyboard_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) {
    if (!circbuf_remaining_read(kbd_buffer)) return 0;
    return circbuf_read(kbd_buffer, size, (uint8_t*)buffer);
}

/**
 * @brief Keyboard poll check
 */
static poll_events_t keyboard_events() {
    return (circbuf_remaining_read(kbd_buffer) ? POLLIN : 0);
}

/**
 * @brief Keyboard poll
 */
static int keyboard_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events) {
    return poll_add(waiter, &kbd_poll_event, events);
}

/**
 * @brief Mouse read
 */
static ssize_t mouse_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) {
    if (!circbuf_remaining_read(mouse_buffer)) return 0;
    return circbuf_read(mouse_buffer, size, (uint8_t*)buffer);
}

/**
 * @brief Mouse poll check
 */
static poll_events_t mouse_events() {
    return (circbuf_remaining_read(mouse_buffer) ? POLLIN : 0);
}

/**
 * @brief Mouse poll
 */
static int mouse_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events) {
    return poll_add(waiter, &mouse_poll_event, events);
}


/**
 * @brief Initialize the peripheral filesystem interface
 */
int periphfs_init() {
    POLL_EVENT_INIT(&kbd_poll_event);
    POLL_EVENT_INIT(&mouse_poll_event);
    kbd_poll_event.checker = keyboard_events;
    mouse_poll_event.checker = mouse_events;
    kbd_buffer = circbuf_create("keyboard buffer", 4096);
    mouse_buffer = circbuf_create("mouse buffer", 4096);
    assert(devfs_register(devfs_root, "keyboard", VFS_CHARDEVICE, &kbd_ops, DEVFS_MAJOR_KEYBOARD, 0, NULL));
    assert(devfs_register(devfs_root, "mouse", VFS_CHARDEVICE, &mouse_ops, DEVFS_MAJOR_MOUSE, 0, NULL));
    return 0;
}


/**
 * @brief Write a new event to the keyboard interface
 * @param event_type The type of event to write
 * @param scancode The scancode relating to the event
 * @returns 0 on success
 */
int periphfs_sendKeyboardEvent(int event_type, key_scancode_t scancode) {
    // TODO: make better
    key_event_t ev = {
        .event_type = event_type,
        .scancode = scancode
    };

    if (circbuf_remaining_write(kbd_buffer) <= (ssize_t)sizeof(key_event_t)) {
        // when circular buffer rewrite comes we can fix this...
        LOG(WARN, "Dropping key event as buffer is full\n");
        return 0;
    }

    circbuf_write(kbd_buffer, sizeof(key_event_t), (uint8_t*)&ev);
    poll_signal(&kbd_poll_event, POLLIN);
    return 0;
}

/**
 * @brief Write a new event to the mouse interface
 * @param event_type The type of event to write
 * @param buttons Buttons being pressed
 * @param x_diff The X difference in the mouse
 * @param y_diff The Y difference in the mouse
 * @param scroll Scroll direction
 */
int periphfs_sendMouseEvent(int event_type, uint32_t buttons, int x_diff, int y_diff, uint8_t scroll) {
    mouse_event_t ev = {
        .event_type = event_type,
        .buttons = buttons,
        .x_difference = x_diff,
        .y_difference = y_diff,
        .scroll = scroll
    };

    if (circbuf_remaining_write(mouse_buffer) <= (ssize_t)sizeof(mouse_event_t)) {
        LOG(WARN, "Dropping mouse event as buffer is full\n");
        return 0;
    }

    circbuf_write(mouse_buffer, sizeof(mouse_event_t), (uint8_t*)&ev);
    poll_signal(&mouse_poll_event, POLLIN);
    return 0;
}   

FS_INIT_ROUTINE(periphfs, INIT_FLAG_DEFAULT, periphfs_init, devfs);