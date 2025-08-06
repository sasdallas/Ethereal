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
    
    if (!(frame->flags & FRAME_NO_BG)) {
        gfx_rect_t r = { .x = x, .y = y, .width = widget->width, .height = widget->height };
        gfx_drawRectangleFilled(ctx, &r, frame->bg_color);
    }
}

/**
 * @brief Create a new frame window based off of the root window
 * @param window The Celestial window to create the frame on
 * @param flags Creation flags
 */
widget_t *frame_createRoot(window_t *window, int flags) {
    widget_t *w = widget_createEmpty();

    widget_frame_t *frame = malloc(sizeof(widget_frame_t));
    frame->bg_color = GFX_RGB(0xfb, 0xfb, 0xfb);
    frame->window = window;
    frame->flags = flags;

    w->type = WIDGET_TYPE_FRAME;
    w->width = window->width;
    w->height = window->height;
    w->children = list_create("widget children");
    w->impl = (void*)frame;
    w->render = frame_render;

    // Create our own geometry that fits the entire window
    widget_renderAtCoordinates(w, 0, 0);
    widget_initEvents(w);

    // Set us as the window driver object
    window->d = (void*)w;

    return w;
}

/**
 * @brief Get context for frame
 * @param frame The frame to get graphics context for
 */
gfx_context_t *frame_getContext(widget_t *w) {
    widget_frame_t *frame = (widget_frame_t*)w->impl;
    if (frame->window) return celestial_getGraphicsContext(frame->window);

    // TODO
    assert(0);
    return NULL;
}