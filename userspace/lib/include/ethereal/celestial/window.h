/**
 * @file userspace/lib/include/ethereal/celestial/window.h
 * @brief window object
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WINDOW_H
#define _CELESTIAL_WINDOW_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <ethereal/celestial/types.h>
#include <structs/hashmap.h>

/**** DEFINITIONS ****/
#define CELESTIAL_DEFAULT_WINDOW_WIDTH      256
#define CELESTIAL_DEFAULT_WINDOW_HEIGHT     256

/**** TYPES ****/

/**
 * @brief Celestial window
 */
typedef struct window {
    wid_t wid;                          // Window ID
    int32_t x;                          // X
    int32_t y;                          // Y
    size_t width;                       // Width
    size_t height;                      // Height
    
    key_t key;                          // SHM key for the buffer
    int shmfd;                          // SHM file descriptor
    uint32_t *buffer;                   // Buffer for the window

hashmap_t *event_handler_map;       // Window event handler map
} window_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new window in Ethereal (undecorated)
 * @param flags The flags to use when creating the window
 * @param width Width of the window
 * @param height Height of the window
 * @returns A window ID or -1 (errno set)
 */
wid_t celestial_createWindowUndecorated(int flags, size_t width, size_t height);

/**
 * @brief Get a window object from an ID
 * @param wid The window ID to get the window for
 * @returns A window object or NULL (errno set)
 */
window_t *celestial_getWindow(wid_t wid);

/**
 * @brief Get a raw framebuffer that you can draw to
 * @param win The window object to get a framebuffer for
 * @returns A framebuffer object or NULL (errno set)
 */
uint32_t *celestial_getFramebuffer(window_t *win);

#endif