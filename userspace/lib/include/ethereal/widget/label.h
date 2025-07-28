/**
 * @file userspace/lib/include/ethereal/widget/label.h
 * @brief Label widget
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_LABEL_H
#define _WIDGET_LABEL_H

/**** INCLUDES ****/
#include <ethereal/widget/widget.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

#define LABEL_DEFAULT_FONT          "/usr/share/DejaVuSans.ttf"
#define LABEL_DEFAULT_COLOR         GFX_RGB(0, 0, 0)

/**** TYPES ****/

typedef struct widget_label {
    char *text;                     // Text of the label
    gfx_color_t color;              // Label color
    size_t font_size;               // Font size of the label
} widget_label_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new label object
 * @param frame The frame to place the label under
 * @param text The text to use for the label
 * @param size The size of font to use for the label
 */
widget_t *label_create(widget_t *frame, char *text, size_t size);

#endif