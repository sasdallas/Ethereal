/**
 * @file userspace/lib/celestial/decor.c
 * @brief Decoration system
 * 
 * Includes Celestial's default theme Mercury.
 * @todo Add support for loading other decorations (from shared libraries?)
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

/* Mercury functions */
extern decor_t *celestial_loadMercury(decor_handler_t *handler, window_t *win);
extern int celestial_initMercury(window_t *win);
extern int celestial_renderMercury(window_t *win);
extern decor_borders_t celestial_getBordersMercury(decor_handler_t *handler);

static decor_handler_t celestial_mercury_theme = {
    .theme = "mercury",
    .load = celestial_loadMercury,
    .borders = celestial_getBordersMercury, 
};

/* Default decoration handler */
decor_handler_t *celestial_default_decor =  &celestial_mercury_theme;

/* Outside of internal window */
#define DECOR_IN_BORDERS(x, y) ((int)x < win->decor->borders.left_width || (int)x > (int)win->info->width - win->decor->borders.right_width || (int)y < win->decor->borders.top_height || (int)y > (int)win->info->height - win->decor->borders.bottom_height)

/* Hack */
int decor_was_last_in_borders = 0; // Was the last mouse state in the borders?

/**
 * @brief Initialize specific decorations for a window
 * @param win The window to initialize decorations on
 * @param decor The decorations to initialize on the window
 */
int celestial_initDecorations(struct window *win, decor_handler_t *decor) {
    win->decor = decor->load(decor, win);
    
    // Temporary shenanigans
    size_t width = win->width;
    size_t height = win->height;
    win->width = CELESTIAL_REAL_WIDTH(win);
    win->height = CELESTIAL_REAL_HEIGHT(win);

    // Setup the window's graphics context
    win->decor->ctx = celestial_getGraphicsContext(win);
    win->ctx = NULL;

    // Setup titlebar too
    win->decor->titlebar = "Celestial Window";

    // Initialize decorations
    win->decor->init(win);
    win->decor->render(win);

    // Create new graphics context for the window
    win->ctx = malloc(sizeof(gfx_context_t));
    // memcpy(win->ctx, win->decor->ctx, sizeof(gfx_context_t));
    memset(win->ctx, 0, sizeof(gfx_context_t));
    win->ctx->width = win->decor->ctx->width;
    win->ctx->pitch = win->decor->ctx->pitch;
    win->ctx->height = win->decor->ctx->height;
    win->ctx->bpp = win->decor->ctx->bpp;
    win->ctx->flags = win->decor->ctx->flags;

    // Move window graphics context
    win->ctx->buffer = &GFX_PIXEL_REAL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
    if (win->decor->ctx->backbuffer) win->ctx->backbuffer = &GFX_PIXEL(win->decor->ctx, win->decor->borders.left_width, win->decor->borders.top_height);
    win->ctx->width -= win->decor->borders.right_width + win->decor->borders.left_width;
    win->ctx->height -= win->decor->borders.bottom_height + win->decor->borders.top_height;
    
    win->width = width;
    win->height = height;
    return 0;
}

/**
 * @brief Initialize the default decorations for Celestial on a window
 * @param win The window to initialize decorations on
 */
int celestial_initDecorationsDefault(struct window *win) {
    return celestial_initDecorations(win, celestial_default_decor);
}

/**
 * @brief Get default decorations
 * @returns The default decorations
 */
decor_handler_t *celestial_getDefaultDecorations() {
    return celestial_default_decor;
}

/**
 * @brief Get boundaries for decoration
 * @param handler The decoration handler to get boundaries for
 */
decor_borders_t celestial_getDecorationBorders(decor_handler_t *handler) {
    return handler->borders(handler);
}

/**
 * @brief Handle a mouse event
 * @param win The window the event occurred on
 * @param event The event to handle
 * @returns 1 if the event was handled and not to pass it to event handler
 */
int celestial_handleDecorationEvent(struct window *win, void *event) {
    celestial_event_header_t *hdr = (celestial_event_header_t*)event;

    // TODO: Adjust event position to ignore borders
    switch (hdr->type) {
        case CELESTIAL_EVENT_MOUSE_BUTTON_DOWN:
            // Depending on bounds
            celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t*)hdr;
            if (DECOR_IN_BORDERS(down->x, down->y) && down->held & CELESTIAL_MOUSE_BUTTON_LEFT) {
                int in_borders = (DECOR_IN_BORDERS(down->x, down->y));
                int b = in_borders ? win->decor->inbtn(win, down->x, down->y) : DECOR_BTN_NONE;

                if (b == DECOR_BTN_CLOSE) {
                    celestial_closeWindow(win);
                    return 0;
                }
            } else {
                return 1;
            }
            return 0;

        case CELESTIAL_EVENT_MOUSE_BUTTON_UP:
            // Depending on bounds
            celestial_event_mouse_button_up_t *up = (celestial_event_mouse_button_up_t*)hdr;
            if (DECOR_IN_BORDERS(up->x, up->y)) {
                celestial_stopDragging(win);
                return 0;
            } else {
                return 1;
            }

        case CELESTIAL_EVENT_MOUSE_MOTION:
            // Depending on bounds
            celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t*)hdr;

            if (DECOR_IN_BORDERS(motion->x, motion->y) || decor_was_last_in_borders) {
                // Check if in bounods
                int in_borders = (DECOR_IN_BORDERS(motion->x, motion->y));
                int b = in_borders ? win->decor->inbtn(win, motion->x, motion->y) : DECOR_BTN_NONE;

                // TODO: Find a better way to do this without spamming state()
                if (b == DECOR_BTN_CLOSE) win->decor->state(win, DECOR_BTN_CLOSE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_CLOSE, DECOR_BTN_STATE_NORMAL);

                if (b == DECOR_BTN_MAXIMIZE) win->decor->state(win, DECOR_BTN_MAXIMIZE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_MAXIMIZE, DECOR_BTN_STATE_NORMAL);

                if (b == DECOR_BTN_MINIMIZE) win->decor->state(win, DECOR_BTN_MINIMIZE, DECOR_BTN_STATE_HOVER);
                else win->decor->state(win, DECOR_BTN_MINIMIZE, DECOR_BTN_STATE_NORMAL);


                // Fix hack
                if (!in_borders) decor_was_last_in_borders = 0;
                else decor_was_last_in_borders = 1;

                return 0;
            } else {
                // Update motion X
                motion->x -= win->decor->borders.left_width;
                motion->y -= win->decor->borders.top_height;
                return 1;
            }

        case CELESTIAL_EVENT_MOUSE_ENTER:
            return 0;

        case CELESTIAL_EVENT_MOUSE_DRAG:
            // Depending on bounds
            celestial_event_mouse_drag_t *drag = (celestial_event_mouse_drag_t*)hdr;
            if (DECOR_IN_BORDERS(drag->x, drag->y)) {
                celestial_startDragging(win);
                return 0;
            } else {
                // Update coordinates
                drag->x -= win->decor->borders.left_width;
                drag->y -= win->decor->borders.top_height;
                drag->win_x += win->decor->borders.left_width;
                drag->win_y += win->decor->borders.top_height;
                return 1;
            }

        case CELESTIAL_EVENT_FOCUSED:
            win->decor->focused = 1;
            win->decor->render(win);
            return 1; // Pass this event along

        case CELESTIAL_EVENT_UNFOCUSED:
            win->decor->focused = 0;
            win->decor->render(win);
            return 1; // Pass this event along

        default:
            return 1;
    }
}

/**
 * @brief Adjust actual X/Y coordinates to be inner window X/Y coordinates
 * Uses decoration bounds to adjust X/Y
 * 
 * @param win The window 
 * @param x X that corresponds to the global window
 * @param y Y that corresponds to the global window
 * @param x_out Output X
 * @param y_out Output Y
 */
void celestial_adjustCoordinates(struct window *win, int32_t x, int32_t y, int32_t *x_out, int32_t *y_out) {
    // Adjust X and Y
    *x_out = x - win->decor->borders.left_width;
    *y_out = y - win->decor->borders.top_height;
}