/**
 * @file userspace/celestialv2/input.c
 * @brief Input system for celestial
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef BUILDING_LINUX

#include "../celestial.h"
#include <ethereal/celestial.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <kernel/fs/periphfs.h>

static int mfd = -1;
static int kfd = -1;


void *mouse_thread(void *arg) {
    TRACE_DEBUG("Enter mouse thread mfd=%d\n", mfd);
    while (1) {
        struct pollfd fds[1];
        fds[0].fd = mfd;
        fds[0].events = POLLIN;
        ssize_t r = poll(fds, 1, -1);
        if (r < 0) FATAL("poll: %s\n", strerror(errno));

        mouse_event_t event;
        ssize_t ret = read(mfd, &event, sizeof(mouse_event_t));
        if (ret < 0) {
            FATAL("read(mfd): %s\n", strerror(errno));
        }
        if (!ret) continue;


        event.x_difference *= 2;
        event.y_difference *= 2;

        // TRACE_DEBUG("Mouse event dx %d dy %d\n", event.x_difference, event.y_difference);
        int mouse_x;
        int mouse_y;
        input_get_mouse_pos(&mouse_x, &mouse_y);
        int nx = mouse_x + event.x_difference;
        int ny = mouse_y - event.y_difference; // must be inverted

        int scroll = 0;
        if (event.scroll == MOUSE_SCROLL_UP) scroll = 1;
        else if (event.scroll == MOUSE_SCROLL_DOWN) scroll = -1;

        // Check events
        mouse_check_events(nx, ny, scroll, event.buttons);
    }
}

void *keyboard_thread(void *arg) {
    while (1) {
        struct pollfd fds[1];
        fds[0].fd = kfd;
        fds[0].events = POLLIN;
        ssize_t r = poll(fds, 1, -1);
        if (r < 0) FATAL("poll: %s\n", strerror(errno));

        key_event_t event;
        ssize_t ret = read(kfd, &event, sizeof(key_event_t));
        if (ret < 0) {
            FATAL("read(kfd): %s\n", strerror(errno));
        }

        if (!ret) continue;

        // TRACE_DEBUG("keyboard event %d\n", event.event_type);
        extern void keyboard_process_event(key_event_t event);
        keyboard_process_event(event);
    }
}

int input_init_backend() {
    // TODO obtain excl. access
    mfd = open("/device/mouse", O_RDONLY);
    if (mfd < 0) FATAL("/device/mouse: %s\n", strerror(errno));
    kfd = open("/device/keyboard", O_RDONLY);
    if (mfd < 0) FATAL("/device/keyboard: %s\n", strerror(errno));

    if (pthread_create(&SERVER->mouse_thread, NULL, mouse_thread, NULL) < 0) {
        FATAL("Could not create mouse input thread: %s\n", strerror(errno));
    }

    if (pthread_create(&SERVER->keyboard_thread, NULL, keyboard_thread, NULL) < 0) {
        FATAL("Could not create keyboard input thread: %s\n", strerror(errno));
    }

    return 0;
}
#endif
