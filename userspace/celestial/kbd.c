/**
 * @file userspace/celestial/kbd.c
 * @brief Keyboard
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <fcntl.h>
#include <unistd.h>
#include <kernel/fs/periphfs.h>

int __celestial_keyboard_fd = -1;

/**
 * @brief Initialize keyboard device
 */
void kbd_init() {
    __celestial_keyboard_fd = open("/device/keyboard", O_RDONLY);
    if (__celestial_keyboard_fd < 0) {
        CELESTIAL_PERROR("open");
        celestial_fatal();
    }
}

/**
 * @brief Update keyboard device
 */
void kbd_update() {
    key_event_t ev;
    ssize_t r = read(__celestial_keyboard_fd, &ev, sizeof(key_event_t));

    while (r > 0) {
        if (WM_FOCUSED_WINDOW) {
            celestial_event_key_t key = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .size = sizeof(celestial_event_key_t),
                .type = CELESTIAL_EVENT_KEY_EVENT,
                .wid = WM_FOCUSED_WINDOW->id,
                .ev = ev,
            };

            event_send(WM_FOCUSED_WINDOW, &key);
        }

        r = read(__celestial_keyboard_fd, &ev, sizeof(key_event_t));
    }

    if (r < 0) {
        CELESTIAL_PERROR("read");
        CELESTIAL_ERR("keyboard: Error while getting event\n");
        celestial_fatal();
    }
}