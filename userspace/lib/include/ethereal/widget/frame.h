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
#include <ethereal/widget/event.h>
#include <ethereal/celestial/window.h>

/**** DEFINITIONS ****/

#define FRAME_DEFAULT           0x0
#define FRAME_NO_BG             0x1 // Don't draw background, so you can render other things

/**** TYPES ****/

typedef struct widget_frame {
    window_t *window;                   // The window (if this is a root frame)
    gfx_color_t bg_color;               // Background color
    widget_event_state_t *event;        // Widget event state
    int flags;
} widget_frame_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new frame window based off of the root window
 * @param window The Celestial window to create the frame on
 * @param flags Creation flags
 */
widget_t *frame_createRoot(window_t *window, int flags);

/**
 * @brief Get context for frame
 * @param frame The frame to get graphics context for
 */
gfx_context_t *frame_getContext(widget_t *w);

#endif