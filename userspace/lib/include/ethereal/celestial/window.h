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
#include <ethereal/celestial/decor.h>
#include <graphics/gfx.h>
#include <structs/hashmap.h>

/**** DEFINITIONS ****/
#define CELESTIAL_DEFAULT_WINDOW_WIDTH      256
#define CELESTIAL_DEFAULT_WINDOW_HEIGHT     256

#define CELESTIAL_WINDOW_FLAG_DECORATED     0x1

#define CELESTIAL_Z_BACKGROUND              0
#define CELESTIAL_Z_DEFAULT                 1
#define CELESTIAL_Z_OVERLAY                 2

/**** TYPES ****/

/**
 * @brief Celestial window
 */
typedef struct window {
    uint8_t flags;                      // Flags

    wid_t wid;                          // Window ID
    int32_t x;                          // X
    int32_t y;                          // Y
    size_t width;                       // Width
    size_t height;                      // Height
    
    key_t key;                          // SHM key for the buffer
    int shmfd;                          // SHM file descriptor
    uint32_t *buffer;                   // Buffer for the window
    gfx_context_t *ctx;                 // Context for the window

    decor_t *decor;                     // Decorations
    uint32_t *decor_buffer;             // Decorations buffers
    decor_window_info_t *info;          // REAL window information. Our macros will use this

    hashmap_t *event_handler_map;       // Window event handler map
} window_t;

/**** MACROS ****/

#define CELESTIAL_REAL_WIDTH(win) ((win->info ? win->info->width : win->width))
#define CELESTIAL_REAL_HEIGHT(win) ((win->info ? win->info->height : win->height))

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
 * @brief Create a new window in Ethereal (decorated)
 * @param flags The flags to use when creating the window
 * @param width Width of the window
 * @param height Height of the window
 * @returns A window ID or -1
 */
wid_t celestial_createWindow(int flags, size_t width, size_t height);

/**
 * @brief Set the title of a decorated window
 * @param window The window to set title of decorated
 * @param title The title to set
 */
void celestial_setTitle(window_t *win, char *title);

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

/**
 * @brief Start dragging a window
 * @param win The window object to start dragging
 * @returns 0 on success or -1 
 * 
 * This will lock the mouse cursor in place and have it continuously drag the window.
 * Usually, unless you have an undecorated window, don't use this.
 */
int celestial_startDragging(window_t *win);

/**
 * @brief Stop dragging a window
 * @param win The window object to stop dragging
 * @returns 0 on success
 * 
 * This will unlock the mouse cursor.
 * Usually, unless you have an undecorated window, don't use this.
 */
int celestial_stopDragging(window_t *win);

/**
 * @brief Initialize graphics for a window
 * @param win The window object to initialize graphics for
 * @param flags Flags to use during context creation
 * @returns Graphics object
 */
gfx_context_t *celestial_initGraphics(window_t *win, int flags);

/**
 * @brief Get graphical context for a window
 * @param win The window object to get the graphics context for
 * @returns Graphics context
 */
gfx_context_t *celestial_getGraphicsContext(window_t *win);

/**
 * @brief Set the position of a window
 * @param win The window object to set position of
 * @param x The X position to set for the window
 * @param y The Y position to set for the window
 * @returns 0 on success
 */
int celestial_setWindowPosition(window_t *win, int32_t x, int32_t y);

/**
 * @brief Set the Z array of a window
 * @param win The window to set the Z array of
 * @param z The Z array to set
 * @returns 0 on success
 */
int celestial_setZArray(window_t *win, int z);

/**
 * @brief Flip a window (specific region)
 * @param win The window to flip
 * @param x The X-coordinate of start region
 * @param y The Y-coordinate of start region
 * @param width The width of the region
 * @param heigth The height of the region
 */
void celestial_flipRegion(window_t *win, int32_t x, int32_t y, size_t width, size_t height);

/**
 * @brief Flip/update a window
 * @param win The window to flip
 */
void celestial_flip(window_t *win);

/**
 * @brief Close a window
 * @param win The window to close
 * 
 * @warning Any attempts to update the window from here might crash.
 */
void celestial_closeWindow(window_t *win);

#endif
