/**
 * @file userspace/lib/widget/label.c
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

#include <ethereal/widget.h>
#include <stdlib.h>
#include <string.h>

/* TEMPORARY: Default font for label */
gfx_font_t *label_default_font = NULL;

/**
 * @brief Render the label object
 */
static void label_render(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y) {
    widget_label_t *label = (widget_label_t*)widget->impl;
    gfx_renderString(ctx, label_default_font, label->text, x, y, label->color);
}

/**
 * @brief Create a new label object
 * @param frame The frame to place the label under
 * @param text The text to use for the label
 * @param size The size of font to use for the label
 */
widget_t *label_create(widget_t *frame, char *text, size_t size) {
    if (!label_default_font) {
        // TODO: The graphics API no longer uses context
        label_default_font = gfx_loadFont(NULL, LABEL_DEFAULT_FONT);
    }
    
    widget_t *widget = widget_createEmpty();

    widget_label_t *label = malloc(sizeof(widget_label_t));
    label->text = strdup(text);
    label->font_size = size;
    label->color = LABEL_DEFAULT_COLOR;

    widget->render = label_render;
    widget->type = WIDGET_TYPE_LABEL;
    widget->width = size * strlen(text);
    widget->height = size;
    widget->impl = (void*)label;

    list_append(frame->children, widget);
    return widget;
}