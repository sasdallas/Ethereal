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
#include <structs/hashmap.h>
#include <ethereal/shared.h>

/**** DEFINITIONS ****/

#define CELESTIAL_MAX_WINDOW_ID                 32767
#define CELESTIAL_WINDOW_REDRAW_THRESHOLD       10


#define WINDOW_STATE_NORMAL                     0
#define WINDOW_STATE_DRAGGING                   1
#define WINDOW_STATE_RESIZING                   2
#define WINDOW_STATE_CLOSING                    3
#define WINDOW_STATE_OPENING                    4
#define WINDOW_STATE_CLOSED                     5


#define WINDOW_ANIM_NONE                        0
#define WINDOW_ANIM_OPENING                     1
#define WINDOW_ANIM_CLOSING                     2


/**** TYPES ****/

typedef struct wm_window {
    pid_t id;                   // ID of the window
    int sock;                   // Socket ID
    int32_t x;                  // X position of the window
    int32_t y;                  // Y position of the window
    size_t width;               // Width of the window
    size_t height;              // Height of the window
    uint32_t events;            // Events the window subscribed to
    uint8_t state;              // Current window state
    sprite_t *sp;               // Window sprite
    uint8_t animation;          // Animation ongoing
    uint64_t anim_start;        // Animation start

    int z_array;                // Z array

    uint8_t *buffer;            // Buffer allocated to the window
    key_t bufkey;               // Buffer shared memory key
    int shmfd;                  // Buffer shared memory fd
} wm_window_t;

typedef struct wm_update_window {
    wm_window_t *win;           // Window
    gfx_rect_t rect;            // Rectangle to draw
} wm_update_window_t;

/**** VARIABLES ****/

extern hashmap_t *__celestial_window_map;
extern list_t *__celestial_window_update_queue;

/**** MACROS ****/

#define WID_EXISTS(wid) (hashmap_has(__celestial_window_map, (void*)(uintptr_t)wid))
#define WID(wid) ((wm_window_t*)hashmap_get(__celestial_window_map, (void*)(uintptr_t)wid))
#define WID_BELONGS_TO_SOCKET(wid, sock) (WID(wid)->sock == sock)

/**** FUNCTIONS ****/

/**
 * @brief Initialize the window system
 */
void window_init();

/**
 * @brief Create a new window in the window system
 * @param sock Socket creating the window
 * @param flags Flags for window creation
 * @param width Width
 * @param height Height
 * @returns A new window object
 */
wm_window_t *window_new(int sock, int flags, size_t width, size_t height);

/**
 * @brief Redraw all windows 
 */
void window_redraw();

/**
 * @brief Get the topmost window the mouse is in
 * @param x The X coordinate
 * @param y The Y coordinate
 */
wm_window_t *window_top(int32_t x, int32_t y);

/**
 * @brief Add a window to the global update queue
 * @param win The window to add to the update queue
 * @param rect The rectangle of the window to draw
 */
int window_update(wm_window_t *win, gfx_rect_t rect);

/**
 * @brief Update an entire damaged region
 * @param rect Rectangle encompassing the damaged region
 */
void window_updateRegion(gfx_rect_t rect);

/**
 * @brief Close a window
 * @param win The window to close
 */
void window_close(wm_window_t *win);

/**
 * @brief Resize a window to a desired width/height
 * 
 * In Celestial, windows are resized using a complex process:
 *      - Client sends @c CELESTIAL_REQ_RESIZE with the desired width and height
 *      - Server receives, sets window state to @c WINDOW_STATE_RESIZING and creates the new shared memory object
 *      - If pending flip requests are present, resizing is delayed and the process is done asyncronously
 *      - The buffers are switched, Celestial sends @c CELESTIAL_EVENT_RESIZE with the new buffer key and data, allowing the window to process the event
 *      - A response to the original @c CELESTIAL_REQ_RESIZE is sent to indicate success and return execution
 * 
 * @param win The window to resize
 * @param new_width The new width of the window
 * @param new_height The new height of the window
 */
int window_resize(wm_window_t *win, size_t new_width, size_t new_height);

/**
 * @brief Update an entire damaged region
 * @param rect Rectangle encompassing the damaged region
 * @param win A window to ignore if needed
 */
void window_updateRegionIgnoring(gfx_rect_t rect, wm_window_t *ign);

/**
 * @brief Begin a window animation
 * @param window The window to begin on
 * @param anim The animation to begin
 */
void window_beginAnimation(wm_window_t *win, int anim);

#endif