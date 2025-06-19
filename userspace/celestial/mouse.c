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

/* Mouse sprite */
sprite_t *__celestial_mouse_sprite = NULL;

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

    // int32_t last_mouse_x = WM_MOUSEX;
    // int32_t last_mouse_y = WM_MOUSEY;

    // Update X and Y
    WM_MOUSEX += event.x_difference;
    WM_MOUSEY -= event.y_difference; // TODO: Maybe add kernel flag to invert this or do it in driver

    // Do we need to adjust?
    if (WM_MOUSEX < 0) WM_MOUSEX = 0;
    if (WM_MOUSEY < 0) WM_MOUSEY = 0;
    if ((size_t)WM_MOUSEX >= GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width-1) WM_MOUSEX = GFX_WIDTH(WM_GFX)-WM_MOUSE_SPRITE->width;
    if ((size_t)WM_MOUSEY >= GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height) WM_MOUSEY = GFX_HEIGHT(WM_GFX)-WM_MOUSE_SPRITE->height;

    // Make clips
    // if (last_mouse_x != WM_MOUSEX || last_mouse_y != WM_MOUSEY) gfx_createClip(WM_GFX, last_mouse_x, last_mouse_y, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
    gfx_createClip(WM_GFX, WM_MOUSEX, WM_MOUSEY, WM_MOUSE_SPRITE->width, WM_MOUSE_SPRITE->height);
}

/**
 * @brief Render the mouse
 */
void mouse_render() {
    gfx_renderSprite(WM_GFX, WM_MOUSE_SPRITE, WM_MOUSEX, WM_MOUSEY);
}
