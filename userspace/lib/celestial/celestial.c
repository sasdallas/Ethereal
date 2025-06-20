/**
 * @file userspace/lib/celestial/celestial.c
 * @brief Main celestial functions
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <stdlib.h>

/**
 * @brief Main loop for a Celestial window
 */
void celestial_mainLoop() {
    // For now, just collect events
    while (1) {
        void *resp = celestial_getResponse(-1);
        free(resp);
    }
}