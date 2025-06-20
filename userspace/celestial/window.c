/**
 * @file userspace/celestial/window.c
 * @brief Window logic
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include "event.h"

/* Window map */
hashmap_t *__celestial_window_map = NULL;

/* Window list */
list_t *__celestial_window_list = NULL;

/* Window ID bitmap */
uint32_t window_id_bitmap[CELESTIAL_MAX_WINDOW_ID/4] = { 0 };

/* Last redraw time */
uint64_t __celestial_last_redraw_time = 0;

/**
 * @brief Allocate a new ID for a window
 */
static pid_t window_allocateID() {
    for (uint32_t i = 0; i < CELESTIAL_MAX_WINDOW_ID/4; i++) {
        for (uint32_t j = 0; j < sizeof(uint32_t) * 8; j++) {
            // Check each bit in the ID bitmap
            if (!(window_id_bitmap[i] & (1 << j))) {
                // This is a free ID, allocate and return it
                window_id_bitmap[i] |= (1 << j);
                return (i * (sizeof(uint32_t) * 8)) + j;
            }
        }
    }

    CELESTIAL_LOG("Out of window PIDs\n");
    return -1;
}

/**
 * @brief Free a ID for a window
 * @param id The ID to free
 */
static void window_freeID(pid_t id) {
    int bitmap_idx = (id / 32) * 32;
    window_id_bitmap[bitmap_idx] &= ~(1 << (id - bitmap_idx));
} 

/**
 * @brief Create a new window in the window system
 * @param sock Socket creating the window
 * @param flags Flags for window creation
 * @param width Width
 * @param height Height
 * @returns A new window object
 */
wm_window_t *window_new(int sock, int flags, size_t width, size_t height) {
    wm_window_t *win = malloc(sizeof(wm_window_t));
    win->id = window_allocateID();
    win->sock = sock;
    win->width = width;
    win->height = height;
    win->x = GFX_WIDTH(WM_GFX) / 2 - (width/2);
    win->y = GFX_HEIGHT(WM_GFX) / 2 - (height/2);
    win->width = width;
    win->height = height;
    win->events = CELESTIAL_EVENT_DEFAULT_SUBSCRIBED;

    // Make buffer for it
    win->shmfd = shared_new(win->height * win->width * 4, SHARED_DEFAULT);
    win->bufkey = shared_key(win->shmfd);
    win->buffer = mmap(NULL, win->height * win->width * 4, PROT_READ | PROT_WRITE, MAP_SHARED, win->shmfd, 0);

    CELESTIAL_DEBUG("New window %dx%d at X %d Y %d SHM KEY %d created\n", win->width, win->height, win->x, win->y, win->bufkey);

    list_append(WM_WINDOW_LIST, win);
    hashmap_set(WM_WINDOW_MAP, (void*)(uintptr_t)win->id, win);
    return win;
}

/**
 * @brief Initialize the window system
 */
void window_init() {
    WM_WINDOW_MAP = hashmap_create_int("celestial window map", 20);
    WM_WINDOW_LIST = list_create("celestial window list");
}

/**
 * @brief Redraw all windows 
 */
void window_redraw() {
    // Get the current time
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t curtime = (tv.tv_sec * 1000) + (tv.tv_usec);
    uint64_t threshold = curtime - __celestial_last_redraw_time;

    int time_to_redraw = (threshold > CELESTIAL_WINDOW_REDRAW_THRESHOLD);
    if (!time_to_redraw) return;


    // Update redraw time
    __celestial_last_redraw_time = curtime;

    // We need to redraw all our windows
    // TODO: Z order, we are just drawing all of them
    // TODO: Use our clipping system to allow windows to be marked for drawing. They can call celestial_flip() to redraw their content (and will only be redrawn if they call flip())
    foreach(window_node, WM_WINDOW_LIST) {
        wm_window_t *win = (wm_window_t*)window_node->value;
    
        // Make a sprite and render it
        sprite_t sp = {
            .bitmap = (uint32_t*)win->buffer,
            .width = win->width,
            .height = win->height,
        };

        gfx_createClip(WM_GFX, win->x, win->y, win->width, win->height);
        gfx_renderSprite(WM_GFX, &sp, win->x, win->y);
    }
}

/**
 * @brief Get the topmost window the mouse is in
 */
wm_window_t *window_top(int32_t x, int32_t y) {
    // TODO: Z ordering

    foreach(window_node, WM_WINDOW_LIST) {
        wm_window_t *win = (wm_window_t*)window_node->value;

        if (win->x <= x && (int)(win->x + win->width) > x && win->y <= y && (int)(win->y + win->height) > y) {
            return win;
        }
    }

    return NULL;
}