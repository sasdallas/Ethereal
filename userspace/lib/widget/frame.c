/**
 * @file userspace/lib/widget/frame.c
 * @brief Frame widget
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

/**
 * @brief Render callback for frame
 */
static void frame_render(widget_t *widget, gfx_context_t *ctx, int32_t x, int32_t y) {
    widget_frame_t *frame = (widget_frame_t*)widget->impl;
    gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
    gfx_drawRectangleFilled(ctx, &r, frame->bg_color);
}

/**
 * @brief Create a new frame window based off of the root window
 * @param window The Celestial window to create the frame on
 */
widget_t *frame_createRoot(window_t *window) {
    widget_t *w = widget_createEmpty();

    widget_frame_t *frame = malloc(sizeof(widget_frame_t));
    frame->bg_color = GFX_RGB(0xfe, 0xfc, 0xff);
    frame->window = window;

    w->type = WIDGET_TYPE_FRAME;
    w->width = window->width;
    w->height = window->height;
    w->children = list_create("widget children");
    w->impl = (void*)frame;
    w->render = frame_render;

    // Create our own geometry that fits the entire window
    widget_renderAtCoordinates(w, 0, 0);

    return w;
}