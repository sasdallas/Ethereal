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
#include <unistd.h>
#include <immintrin.h>

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
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__NO_SSE)
            uint32_t *src_row = (uint32_t*)(WM_GFX->backbuffer + (((_y + upd->win->y) * WM_GFX->pitch) + (upd->rect.x + upd->win->x) * (WM_GFX->bpp / 8)));
            uint32_t *dst_row = &buf[upd->win->width * _y + upd->rect.x];

            for (uint32_t _x = 0; _x < upd->rect.width; _x += 4) {
                __m128i src_pixels = _mm_loadu_si128((__m128i*)&src_row[_x]);
                __m128i dst_pixels = _mm_loadu_si128((__m128i*)&dst_row[_x]);

                // Extract alpha channel from destination pixels
                __m128i alpha = _mm_srli_epi32(dst_pixels, 24);

                // Check if alpha is 255 (fully opaque)
                __m128i mask = _mm_cmpeq_epi32(alpha, _mm_set1_epi32(255));

                // Extract alpha channel from source pixels
                __m128i src_alpha = _mm_srli_epi32(src_pixels, 24);

                // Compute inverse alpha (255 - alpha)
                __m128i inv_alpha = _mm_sub_epi32(_mm_set1_epi32(255), src_alpha);

                // Multiply source and destination by their respective alpha values
                __m128i src_mul = _mm_mullo_epi16(_mm_and_si128(src_pixels, _mm_set1_epi32(0x00FF00FF)), src_alpha);
                __m128i dst_mul = _mm_mullo_epi16(_mm_and_si128(dst_pixels, _mm_set1_epi32(0x00FF00FF)), inv_alpha);

                // Add the results and normalize
                __m128i blended = _mm_add_epi16(src_mul, dst_mul);
                blended = _mm_srli_epi16(blended, 8);

                // Combine the blended RGB channels with the source alpha channel
                blended = _mm_or_si128(_mm_and_si128(blended, _mm_set1_epi32(0x00FF00FF)), _mm_slli_epi32(src_alpha, 24));

                // Blend only where alpha is not 255
                __m128i result = _mm_or_si128(_mm_and_si128(mask, dst_pixels),
                                _mm_andnot_si128(mask, blended));

                _mm_storeu_si128((__m128i*)& src_row[_x], result);
            }
#else
            // Normal, loser non-SSE copy
            for (uint32_t _x = upd->rect.x; _x < upd->rect.x + upd->rect.width; _x++) {
                gfx_color_t *src_pixel = (uint32_t*)(WM_GFX->backbuffer + (((_y + upd->win->y) * WM_GFX->pitch) + ((_x + upd->win->x) * (WM_GFX->bpp/8))));
                gfx_color_t *dst_pixel = &buf[upd->win->width * _y + _x];

                // Blend
                if (GFX_RGB_A(*dst_pixel) == 255) {
                    *src_pixel = *dst_pixel;
                } else {
                    *src_pixel = gfx_alphaBlend(*dst_pixel, *src_pixel);
                }
            }
#endif
        }

        // HACK: Is the window closing?
        if (upd->win && upd->win->state == WINDOW_STATE_CLOSING) {
            close(upd->win->shmfd);
            free(upd->win);
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

    // TODO: Overlay
} 

/**
 * @brief Close a window
 * @param win The window to close
 */
void window_close(wm_window_t *win) {
    win->state = WINDOW_STATE_CLOSING;

    // TODO: Animation
    switch (win->z_array) {
        case CELESTIAL_Z_BACKGROUND:
            list_delete(WM_WINDOW_LIST_BG, list_find(WM_WINDOW_LIST_BG, win));
            break;

        case CELESTIAL_Z_OVERLAY:
            list_delete(WM_WINDOW_LIST_OVERLAY, list_find(WM_WINDOW_LIST_OVERLAY, win));
            break;

        case CELESTIAL_Z_DEFAULT:
        default:
            list_delete(WM_WINDOW_LIST, list_find(WM_WINDOW_LIST, win));
            break;    
    }

    // Window is removed from, flip it now
    window_updateRegion((gfx_rect_t){ .x = win->x, .y = win->y, .width = win->width, .height = win->height });
}

/**
 * @brief Resize a window to a desired width/height
 * 
 * In Celestial, windows are resized using a complex process:
 *      - Client sends @c CELESTIAL_REQ_RESIZE with the desired width and height
 *      - Server receives, sets window state to @c WINDOW_STATE_RESIZING and creates the new shared memory object
 *      - If pending flip requests are present, resizing is delayed and the process is done asyncronously
 *      - The buffers are switched, Celestial sends @c CELESTIAL_EVENT_RESIZE with the new buffer key and data, allowing the window to process the event
 *      - If the window position was required to be updated, Celestial also sends @c CELESTIAL_EVENT_POSITION_UPDATE to notify the changed position.
 *      - A response to the original @c CELESTIAL_REQ_RESIZE is sent to indicate success and return execution
 * 
 * @param win The window to resize
 * @param new_width The new width of the window
 * @param new_height The new height of the window
 */
int window_resize(wm_window_t *win, size_t new_width, size_t new_height) {
    // Validate parameters
    int pos_modified = 0;

    size_t old_buffer_size = win->width * win->height * 4;
    gfx_rect_t update_region = { .x = win->x, .y = win->y, .width = win->width, .height = win->height };

    if (win->x + new_width >= GFX_WIDTH(WM_GFX)) {
        if (new_width >= GFX_WIDTH(WM_GFX)) return -EINVAL;
        pos_modified = 1;
        win->x = GFX_WIDTH(WM_GFX) - new_width;
    }
    
    if (win->y + new_height >= GFX_HEIGHT(WM_GFX)) {
        if (new_height >= GFX_HEIGHT(WM_GFX)) return -EINVAL;
        pos_modified = 1;
        win->y = GFX_HEIGHT(WM_GFX) - new_height;
    }

    // TODO: Flush pending buffers, implement asyncronous resize, etc.

    // Set them in the window
    win->width = new_width;
    win->height = new_height;

    // Create a new buffer key
    int new_shm_object = shared_new(win->width * win->height * 4, SHARED_DEFAULT);
    win->bufkey = shared_key(new_shm_object);

    // Unmap the old buffer from memory
    munmap(win->buffer, old_buffer_size);

    // Close the old shared memory object. This decrements reference count.
    close(win->shmfd);
    win->shmfd = new_shm_object;

    // Map the new buffer into memory
    win->buffer = mmap(NULL, win->width * win->height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, win->shmfd, 0);

    // Resize is complete, mostly. Send event to server
    celestial_event_resize_t resize_event = {
        .magic = CELESTIAL_MAGIC_EVENT,
        .type = CELESTIAL_EVENT_RESIZE,
        .size = sizeof(celestial_event_resize_t),
        .wid = win->id,
        .new_width = win->width,
        .new_height = win->height,
        .buffer_key = win->bufkey
    };

    event_send(win, &resize_event);

    // Do we need to adjust window position?
    if (pos_modified) {
        celestial_event_position_change_t pos_change = {
            .magic = CELESTIAL_MAGIC_EVENT,
            .type = CELESTIAL_EVENT_POSITION_CHANGE,
            .size = sizeof(celestial_event_position_change_t),
            .wid = win->id,
            .x = win->x,
            .y = win->y
        };

        event_send(win, &pos_change);
    }

    // Update the old region where the window was
    window_updateRegion(update_region);

    // Resize process is complete
    return 0;
}