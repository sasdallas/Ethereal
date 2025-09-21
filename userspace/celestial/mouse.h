/**
 * @file userspace/celestial/mouse.h
 * @brief Mouse
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_MOUSE_H
#define _CELESTIAL_WM_MOUSE_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define CELESTIAL_DEFAULT_MOUSE_CURSOR "/usr/share/cursor.bmp"

/**** FUNCTIONS ****/

/**
 * @brief Initialize the mouse system
 * @returns 0 on success
 */
int mouse_init();

/**
 * @brief Update the mouse (non-blocking)
 */
int mouse_update();

/**
 * @brief Render the mouse
 */
void mouse_render();

/**
 * @brief Change mouse sprite
 * @param target The target sprite to change to
 */
void mouse_change(int target);

#endif