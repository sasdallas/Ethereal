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

/* Window list */
list_t *__celestial_window_list = NULL;

/* Window ID bitmap */
uint32_t window_id_bitmap[CELESTIAL_MAX_WINDOW_ID/4] = { 0 };

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
 * @brief Initialize the window system
 */
void window_init() {
    WM_WINDOW_LIST = list_create("window list");
}