/**
 * @file userspace/lib/include/ethereal/celestial.h
 * @brief Celestial header file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_H
#define _CELESTIAL_H

/**** INCLUDES ****/
#include <ethereal/celestial/request.h>
#include <ethereal/celestial/window.h>
#include <ethereal/celestial/event.h>
#include <ethereal/celestial/decor.h>

/**** TYPES ****/

typedef struct celestial_info {
    size_t screen_width;            // Screen width
    size_t screen_height;           // Screen height
} celestial_info_t;

/**** FUNCTIONS ****/

/**
 * @brief Main loop for a Celestial window
 * 
 * You are not required to enter this. This is if you have async threads running and don't want to care.
 * Most likely, you want @c celestial_poll that will handle updates on your window.
 */
void celestial_mainLoop();

/**
 * @brief Get Celestial window server information
 * @returns Celestial server information
 */
celestial_info_t *celestial_getServerInformation();

#endif