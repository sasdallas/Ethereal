/**
 * @file userspace/lib/include/ethereal/widget/frame.h
 * @brief Main frame widget of ethereal
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_FRAME_H
#define _WIDGET_FRAME_H

/**** INCLUDES ****/
#include <stdint.h>
#include <ethereal/widget/widget.h>
#include <ethereal/celestial/window.h>

/**** TYPES ****/

typedef struct widget_frame {
    window_t *window;                   // The window (if this is a root frame)
    gfx_color_t bg_color;               // Background color
} widget_frame_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new frame window based off of the root window
 * @param window The Celestial window to create the frame on
 */
widget_t *frame_createRoot(window_t *window);

#endif