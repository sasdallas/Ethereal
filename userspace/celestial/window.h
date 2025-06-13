/**
 * @file userspace/celestial/window.h
 * @brief Celestial window
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_WINDOW_H
#define _CELESTIAL_WM_WINDOW_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

#define CELESTIAL_MAX_WINDOW_ID     32767

/**** TYPES ****/

typedef struct wm_window {
    pid_t id;                   // ID of the window
    int sock;                   // Socket ID
    int32_t x;                  // X position of the window
    int32_t y;                  // Y position of the window
    size_t width;               // Width of the window
    size_t height;              // Height of the window
    gfx_context_t *ctx;         // Context allocated to the window
} wm_window_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the window system
 */
void window_init();

#endif