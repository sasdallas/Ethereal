/**
 * @file userspace/celestial/mouse.c
 * @brief Mouse system
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
#include <unistd.h>
#include <errno.h>
#include <kernel/fs/periphfs.h>

/* Mouse file descriptor */
int __celestial_mouse_fd = 0;

/* Current mouse position */
int __celestial_mouse_x = 0;
int __celestial_mouse_y = 0;

/* Last X/Y */
static int last_mouse_x = 0;
static int last_mouse_y = 0;

/* Mouse sprite */
sprite_t *__celestial_mouse_sprite = NULL;

/* Selected window */
wm_window_t *__celestial_mouse_window = NULL;

/* Currently/previously held mouse buttons */
uint32_t __celestial_mouse_buttons = 0;
uint32_t __celestial_previous_buttons = 0;

/**
 * @brief Initialize the mouse system
 * @returns 0 on success
 */
int mouse_init() {
    WM_MOUSE = open("/device/mouse", O_RDONLY);
    if (WM_MOUSE < 0) {
        CELESTIAL_PERROR("open");
        celestial_fatal();
    }

    // Open mouse cursor
    FILE *f = fopen(CELESTIAL_DEFAULT_MOUSE_CURSOR, "r");
    if (!f) {
        // TODO: Maybe we can use a placeholder
        CELESTIAL_ERR("mouse: Could not open mouse cursor image at " CELESTIAL_DEFAULT_MOUSE_CURSOR "\n");
        celestial_fatal();
    }

    __celestial_mouse_sprite = gfx_createSprite(25, 25);
    if (gfx_loadSprite(__celestial_mouse_sprite, f)) {
        CELESTIAL_ERR("mouse: Failed to load mouse sprite (using " CELESTIAL_DEFAULT_MOUSE_CURSOR ")\n");
        celestial_fatal();
    }

    // Default to center of screen
    WM_MOUSEX = GFX_WIDTH(WM_GFX) / 2;
    WM_MOUSEY = GFX_HEIGHT(WM_GFX) / 2;

    gfx_createClip(WM_GFX, WM_MOUSEX, WM_MOUSEY, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
    gfx_renderSprite(WM_GFX, WM_MOUSE_SPRITE, WM_MOUSEX, WM_MOUSEY);

    return 0;
}

/**
 * @brief Send any mouse events
 */
void mouse_events() {
    // Check to make sure still in window
    if (WM_MOUSE_WINDOW) {
        if (!(WM_MOUSE_WINDOW->x <= WM_MOUSEX) || !((int)(WM_MOUSE_WINDOW->x + WM_MOUSE_WINDOW->width) > WM_MOUSEX) ||
                !(WM_MOUSE_WINDOW->y <= WM_MOUSEY) || !((int)(WM_MOUSE_WINDOW->y + WM_MOUSE_WINDOW->height) > WM_MOUSEY)) {
           
            // Send exit event
            celestial_event_mouse_exit_t exit = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_EXIT,
                .size = sizeof(celestial_event_mouse_exit_t),
                .wid = WM_MOUSE_WINDOW->id
            };

            event_send(WM_MOUSE_WINDOW, &exit);
            WM_MOUSE_WINDOW = NULL;
        }
    }

    // Send corresponding event
    if (!WM_MOUSE_WINDOW) {
        // Try to get a new window
        WM_MOUSE_WINDOW = window_top(WM_MOUSEX, WM_MOUSEY);
        if (WM_MOUSE_WINDOW) {
            // Send new event
            celestial_event_mouse_enter_t enter = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_ENTER,
                .size = sizeof(celestial_event_mouse_enter_t),
                .wid = WM_MOUSE_WINDOW->id,
                .x = WM_MOUSEX,
                .y = WM_MOUSEY
            };

            event_send(WM_MOUSE_WINDOW, &enter);
        }
    } else {
        // Check if we got a press
        if (WM_MOUSE_BUTTONS != __celestial_previous_buttons) {
            if ((WM_MOUSE_BUTTONS & __celestial_previous_buttons) < __celestial_previous_buttons) {
                CELESTIAL_DEBUG("mouse: Button released\n");

                // Determine released button
                uint32_t btn_released = __celestial_previous_buttons & ~(WM_MOUSE_BUTTONS);

                if (btn_released == 0x3 || btn_released > CELESTIAL_MOUSE_BUTTON_MIDDLE) {
                    // Debug check to make sure we can't hit two at once
                    CELESTIAL_ERR("mouse: Released two buttons at the same time (0x%x)! Forgetting about event.\n", btn_released);
                } else {
                    // Send!
                    celestial_event_mouse_button_up_t up = {
                        .magic = CELESTIAL_MAGIC_EVENT,
                        .type = CELESTIAL_EVENT_MOUSE_BUTTON_UP,
                        .size = sizeof(celestial_event_mouse_button_up_t),
                        .wid = WM_MOUSE_WINDOW->id,
                        .x = WM_MOUSEX,
                        .y = WM_MOUSEY,
                        .released = btn_released
                    };

                    event_send(WM_MOUSE_WINDOW, &up);
                }
            } else {
                CELESTIAL_DEBUG("mouse: Button pressed\n");

                // Determine pressed button
                uint32_t btn_pressed = WM_MOUSE_BUTTONS & ~(__celestial_previous_buttons);
                if (btn_pressed == 0x3 || btn_pressed > CELESTIAL_MOUSE_BUTTON_MIDDLE) {
                    // Debug check to make sure we can't hit two at once
                    CELESTIAL_ERR("mouse: Pressed two buttons at the same time (0x%x)! Forgetting about event.\n", btn_pressed);
                } else {
                    // Send!
                    celestial_event_mouse_button_down_t down = {
                        .magic = CELESTIAL_MAGIC_EVENT,
                        .type = CELESTIAL_EVENT_MOUSE_BUTTON_DOWN,
                        .size = sizeof(celestial_event_mouse_button_down_t),
                        .wid = WM_MOUSE_WINDOW->id,
                        .x = WM_MOUSEX,
                        .y = WM_MOUSEY,
                        .held = btn_pressed,
                    };

                    event_send(WM_MOUSE_WINDOW, &down);
                }
            }
        }

        // Send motion event if there was any
        if (WM_MOUSEX != last_mouse_x || WM_MOUSEY != last_mouse_y) {
            celestial_event_mouse_motion_t motion = {
                .magic = CELESTIAL_MAGIC_EVENT,
                .type = CELESTIAL_EVENT_MOUSE_MOTION,
                .size = sizeof(celestial_event_mouse_motion_t),
                .wid = WM_MOUSE_WINDOW->id,
                .x = WM_MOUSEX,
                .y = WM_MOUSEY,
                .buttons = WM_MOUSE_BUTTONS,
            };

            event_send(WM_MOUSE_WINDOW, &motion);
        }
    }
}

/**
 * @brief Update and redraw the mouse (non-blocking)
 */
void mouse_update() {
    // Read mouse event
    mouse_event_t event;

    ssize_t r = read(WM_MOUSE, &event, sizeof(mouse_event_t));
    if (r < 0) {
        CELESTIAL_PERROR("read");
        CELESTIAL_ERR("mouse: Error while getting event\n");
        celestial_fatal();
    }

    if (!r) return;

    // Parse the mouse event
    if (event.event_type != EVENT_MOUSE_UPDATE) return;

    last_mouse_x = WM_MOUSEX;
    last_mouse_y = WM_MOUSEY;

    // Update X and Y
    WM_MOUSEX += event.x_difference;
    WM_MOUSEY -= event.y_difference; // TODO: Maybe add kernel flag to invert this or do it in driver

    // Update buttons
    if (WM_MOUSE_BUTTONS != event.buttons) {
        // Translate event.buttons
        uint32_t btns = (event.buttons & MOUSE_BUTTON_LEFT ? CELESTIAL_MOUSE_BUTTON_LEFT : 0) |
                            (event.buttons & MOUSE_BUTTON_RIGHT ? CELESTIAL_MOUSE_BUTTON_RIGHT : 0) |
                            (event.buttons & MOUSE_BUTTON_MIDDLE ? CELESTIAL_MOUSE_BUTTON_MIDDLE : 0);
        
        __celestial_previous_buttons = WM_MOUSE_BUTTONS;
        WM_MOUSE_BUTTONS = btns;
    } else {
        __celestial_previous_buttons = WM_MOUSE_BUTTONS; // To stop spamming mouse press events
    }

    // Do we need to adjust?
    if (WM_MOUSEX < 0) WM_MOUSEX = 0;
    if (WM_MOUSEY < 0) WM_MOUSEY = 0;
    if ((size_t)WM_MOUSEX >= GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width-1) WM_MOUSEX = GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width;
    if ((size_t)WM_MOUSEY >= GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height) WM_MOUSEY = GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height;

    // Did things change?
    if (last_mouse_x != WM_MOUSEX || last_mouse_y != WM_MOUSEY || WM_MOUSE_BUTTONS != __celestial_previous_buttons) {
        mouse_events();
        gfx_createClip(WM_GFX, last_mouse_x, last_mouse_y, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
    }

    // Make clips
    gfx_createClip(WM_GFX, WM_MOUSEX, WM_MOUSEY, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
}

/**
 * @brief Render the mouse
 */
void mouse_render() {
    gfx_renderSprite(WM_GFX, WM_MOUSE_SPRITE, WM_MOUSEX, WM_MOUSEY);
}
