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
#include <kernel/processor_data.h>
#include <structs/ringbuffer.h>
#include <kernel/misc/spinlock.h>

static ssize_t keyboard_read(devfs_node_t *n, loff_t off, size_t size, char *buffer, int flags);
static int keyboard_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t keyboard_poll_events(devfs_node_t *n);
static poll_events_t keyboard_events();

static ssize_t mouse_read(devfs_node_t *n, loff_t off, size_t size, char *buffer, int flags);
static int mouse_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t mouse_events();

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:PERIPHFS", __VA_ARGS__)

static devfs_ops_t kbd_ops = {
    .open = NULL,
    .close = NULL,
    .read_ext = keyboard_read,
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
    .read_ext = mouse_read,
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
RINGBUFFER_DEFINE_STATIC(mouse_buffer, sizeof(mouse_event_t) * 128);
RINGBUFFER_DEFINE_STATIC(kbd_buffer, sizeof(key_event_t) * 128);

/* Locks */
spinlock_t mouse_buffer_lock = SPINLOCK_INITIALIZER;
spinlock_t keyboard_buffer_lock = SPINLOCK_INITIALIZER;

/* Keyboard poll event */
poll_event_t kbd_poll_event;
poll_event_t mouse_poll_event;

/**
 * @brief Keyboard read
 */
static ssize_t keyboard_read(devfs_node_t *n, loff_t off, size_t size, char *buffer, int flags) {
    // We can't allocate while holding a lock, thats why this is so annoyingly complex
    poll_waiter_t *w;
    for (;;) {
        w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &kbd_poll_event, POLLIN);

        // Check if available
        spinlock_acquire(&keyboard_buffer_lock);
        if (ringbuffer_remaining_read(&kbd_buffer)) {
            break;
        }

        if (flags & O_NONBLOCK) {
            spinlock_release(&keyboard_buffer_lock);
            return -EAGAIN;
        }
        spinlock_release(&keyboard_buffer_lock);
        
        // Wait
        int ret = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            return ret;
        }
    }

    ssize_t r = ringbuffer_read(&kbd_buffer, buffer, size);
    spinlock_release(&keyboard_buffer_lock);

    // can only free now ugh
    poll_exit(w);
    poll_destroyWaiter(w);

    return r;
}

/**
 * @brief Keyboard poll check
 */
static poll_events_t keyboard_events() {
    spinlock_acquire(&keyboard_buffer_lock);
    poll_events_t ret = (ringbuffer_remaining_read(&kbd_buffer) ? POLLIN : 0);
    spinlock_release(&keyboard_buffer_lock);
    return ret;
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
static ssize_t mouse_read(devfs_node_t *n, loff_t off, size_t size, char *buffer, int flags) {
    // We can't allocate while holding a lock, thats why this is so annoyingly complex
    poll_waiter_t *w;
    for (;;) {
        w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &mouse_poll_event, POLLIN);

        // Check if available
        spinlock_acquire(&mouse_buffer_lock);
        if (ringbuffer_remaining_read(&mouse_buffer)) {
            
            break;
        }

        if (flags & O_NONBLOCK) {
            spinlock_release(&mouse_buffer_lock);
            return -EAGAIN;
        }
        spinlock_release(&mouse_buffer_lock);
        
        // Wait
        int ret = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            return ret;
        }
    }

    ssize_t r = ringbuffer_read(&mouse_buffer, buffer, size);
    spinlock_release(&mouse_buffer_lock);

    // can only free now ugh
    poll_exit(w);
    poll_destroyWaiter(w);

    return r;
}

/**
 * @brief Mouse poll check
 */
static poll_events_t mouse_events() {
    spinlock_acquire(&mouse_buffer_lock);
    poll_events_t ret = (ringbuffer_remaining_read(&mouse_buffer) ? POLLIN : 0);
    spinlock_release(&mouse_buffer_lock);
    return ret;
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

    spinlock_acquire(&keyboard_buffer_lock);
    ringbuffer_write(&kbd_buffer, (char*)&ev, sizeof(key_event_t));
    spinlock_release(&keyboard_buffer_lock);
    poll_signal(&kbd_poll_event, POLLIN);
    return 0;
}

/**
 * @brief Write a new mouse event
 * @param event Mouse event
 */
int periphfs_sendMouseEvent(mouse_event_t *event) {
    spinlock_acquire(&mouse_buffer_lock);
    ringbuffer_write(&mouse_buffer, (char*)event, sizeof(mouse_event_t));
    spinlock_release(&mouse_buffer_lock);
    poll_signal(&mouse_poll_event, POLLIN);
    return 0;
}

/**
 * @brief Write a new relative mouse event to the mouse interface
 * @param buttons Buttons being pressed
 * @param x_diff The X difference in the mouse
 * @param y_diff The Y difference in the mouse
 * @param scroll Scroll direction
 */
int periphfs_sendMouseEventRelative(uint32_t buttons, int x_diff, int y_diff, uint8_t scroll) {
    mouse_event_t ev = {
        .event_type = EVENT_MOUSE_RELATIVE,
        .buttons = buttons,
        .rel = { .x_difference = x_diff, .y_difference = y_diff },
        .scroll = scroll
    };

    return periphfs_sendMouseEvent(&ev);
}


FS_INIT_ROUTINE(periphfs, INIT_FLAG_DEFAULT, periphfs_init, devfs);
