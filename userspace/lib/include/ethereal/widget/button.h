/**
 * @file userspace/lib/include/ethereal/widget/button.h
 * @brief Button widget
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_BUTTON_H
#define _WIDGET_BUTTON_H

/**** INCLUDES ****/
#include <ethereal/widget/widget.h>

/**** DEFINITIONS ****/

#define BUTTON_DISABLED         0
#define BUTTON_ENABLED          1

/**** TYPES ****/

typedef struct widget_button {
    char *text;                 // Text on the button
    gfx_color_t color;          // Button color
    uint8_t enabled;            // Button enabled or disabled
} widget_button_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new button widget
 * @param frame The frame to put the button into
 * @param text The text of the button
 * @param color The primary color of the button
 * @param state The state of the button, @c BUTTON_ENABLED
 */
widget_t *button_create(widget_t *frame, char *text, gfx_color_t color, int state);

#endif