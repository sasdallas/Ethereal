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
#include <kernel/misc/util.h>
#include "event.h"

/* Window map */
hashmap_t *__celestial_window_map = NULL;

/* Window lists */
list_t *__celestial_window_list = NULL;             // Default Z array
list_t *__celestial_window_list_bg = NULL;          // Background Z array
list_t *__celestial_window_list_overlay = NULL;     // Overlay Z array

/* Window update queue */
list_t *__celestial_window_update_queue = NULL;     // Update queue, consists of wm_update_window_t

/* Window ID bitmap */
uint32_t window_id_bitmap[CELESTIAL_MAX_WINDOW_ID/4] = { 0 };

/* Last redraw time */
uint64_t __celestial_last_redraw_time = 0;

/* Focused window */
wm_window_t *__celestial_focused_window = NULL;

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
    win->events = 0x0;
    win->state = WINDOW_STATE_NORMAL;
    win->z_array = CELESTIAL_Z_DEFAULT;

    // Make buffer for it   
    win->shmfd = shared_new(win->height * win->width * 4, SHARED_DEFAULT);
    win->bufkey = shared_key(win->shmfd);
    win->buffer = mmap(NULL, win->height * win->width * 4, PROT_READ | PROT_WRITE, MAP_SHARED, win->shmfd, 0);

    CELESTIAL_DEBUG("New window %dx%d at X %d Y %d SHM KEY %d created\n", win->width, win->height, win->x, win->y, win->bufkey);

    node_t *n = malloc(sizeof(node_t));
    n->value = win;
    n->owner = WM_WINDOW_LIST;
    n->next = WM_WINDOW_LIST->head;
    n->prev = NULL;
    if (WM_WINDOW_LIST->head) WM_WINDOW_LIST->head->prev = n;
    WM_WINDOW_LIST->head = n;
    if (!WM_WINDOW_LIST->tail) WM_WINDOW_LIST->tail = n;

    hashmap_set(WM_WINDOW_MAP, (void*)(uintptr_t)win->id, win);
    return win;
}

/**
 * @brief Initialize the window system
 */
void window_init() {
    WM_WINDOW_MAP = hashmap_create_int("celestial window map", 20);
    WM_WINDOW_LIST = list_create("celestial window list");
    WM_WINDOW_LIST_BG = list_create("celestial background window list");
    WM_WINDOW_LIST_OVERLAY = list_create("celestial overlay window list");
    WM_UPDATE_QUEUE = list_create("celestial update queue");
}

/**
 * @brief Redraw all windows 
 */
void window_redraw() {
    while (1) {
        node_t *n = list_popleft(WM_UPDATE_QUEUE);
        if (!n) break;

        wm_update_window_t *upd = (wm_update_window_t*)n->value;
        free(n);

        // CELESTIAL_DEBUG("window: Redraw window %d (X: %d, Y: %d, W: %d, H: %d)\n", upd->win->id, upd->rect.x, upd->rect.y, upd->rect.width, upd->rect.height);


        // Now we have a rectangle and a window to draw in that rectangle. The rectangle coordinates are relative to the window's
        // Basically replicate the sprite redraw function
        gfx_createClip(WM_GFX, upd->rect.x + upd->win->x, upd->rect.y + upd->win->y, upd->rect.width, upd->rect.height);
        uint32_t *buf = (uint32_t*)upd->win->buffer;
        for (uint32_t _y = upd->rect.y; _y < upd->rect.y + upd->rect.height; _y++) {
            for (uint32_t _x = upd->rect.x; _x < upd->rect.x + upd->rect.width; _x++) {
                GFX_PIXEL(WM_GFX, _x + upd->win->x, _y + upd->win->y) = gfx_alphaBlend(buf[upd->win->width * _y + _x], GFX_PIXEL(WM_GFX, _x + upd->win->x, _y + upd->win->y));
            }
        }
        free(upd);
    }
}

/**
 * @brief Get the topmost window the mouse is in
 */
wm_window_t *window_top(int32_t x, int32_t y) {
    // TODO: We could probably just go through this in reverse...
    wm_window_t *sel = NULL;
    foreach(window_node, WM_WINDOW_LIST) {
        wm_window_t *win = (wm_window_t*)window_node->value;

        if (win->x <= x && (int)(win->x + win->width) > x && win->y <= y && (int)(win->y + win->height) > y) {
            sel = win;
        }
    }

    // foreach(window_node, WM_WINDOW_LIST_BG) {
        // wm_window_t *win = (wm_window_t*)window_node->value;
// 
        // if (win->x <= x && (int)(win->x + win->width) > x && win->y <= y && (int)(win->y + win->height) > y) {
            // return win;
        // }
    // }

    return sel;
}

/**
 * @brief Add a window to the global update queue
 * @param win The window to add to the update queue
 * @param rect The rectangle of the window to draw
 */
int window_update(wm_window_t *win, gfx_rect_t rect) {
    wm_update_window_t *upd = malloc(sizeof(wm_update_window_t));
    upd->win = win;
    memcpy(&upd->rect, &rect, sizeof(gfx_rect_t));
    list_append(WM_UPDATE_QUEUE, upd);
    return 0;
}

/**
 * @brief Update an entire damaged region
 * @param rect Rectangle encompassing the damaged region
 */
void window_updateRegion(gfx_rect_t rect) {
    // First, handle the background update
    foreach(win_node, WM_WINDOW_LIST_BG) {
        wm_window_t *win = (wm_window_t*)win_node->value;

        // Determine if this window collides with our rectanlg
        gfx_rect_t collider = { .x = win->x, .y = win->y, .width = win->width, .height = win->height };
        if (GFX_RECT_COLLIDES(WM_GFX, rect, collider)) {
            // The two rectangles collide
            gfx_rect_t redraw_rect = { .x = rect.x - win->x, .y = rect.y - win->y, .width = rect.width, .height = rect.height };
            window_update(win, redraw_rect);
        }
    }
    
    // Next, handle the other window updates
    foreach(win_node, WM_WINDOW_LIST) {
        wm_window_t *win = (wm_window_t*)win_node->value;

        // Determine if this window collides with our rectangle
        gfx_rect_t collider = { .x = win->x, .y = win->y, .width = win->width, .height = win->height };
        if (GFX_RECT_COLLIDES(WM_GFX, rect, collider)) {
            // The two rectangles collide
            gfx_rect_t redraw_rect = {
                .x = GFX_RECT_MAX(collider.x, rect.x), 
                .y = GFX_RECT_MAX(collider.y, rect.y),
                .width = GFX_RECT_MIN(collider.width, rect.width),
                .height = GFX_RECT_MIN(collider.height, rect.height)
            };

            redraw_rect.width = GFX_RECT_MIN(GFX_RECT_RIGHT(WM_GFX, rect), GFX_RECT_RIGHT(WM_GFX, redraw_rect)) - redraw_rect.x + 1;
            redraw_rect.height = GFX_RECT_MIN(GFX_RECT_BOTTOM(WM_GFX, rect), GFX_RECT_BOTTOM(WM_GFX, redraw_rect)) - redraw_rect.y + 1;

            redraw_rect.x -= win->x;
            redraw_rect.y -= win->y;

            if (redraw_rect.x + redraw_rect.width > win->width) redraw_rect.width = win->width - redraw_rect.x;
            if (redraw_rect.y + redraw_rect.height > win->height) redraw_rect.height = win->height - redraw_rect.y;

            // CELESTIAL_DEBUG("window: collision in WID %d (X: %d, Y: %d, W: %d, H: %d)\n", win->id, redraw_rect.x, redraw_rect.y, redraw_rect.width, redraw_rect.height);
            window_update(win, redraw_rect);
        }
    }
} 