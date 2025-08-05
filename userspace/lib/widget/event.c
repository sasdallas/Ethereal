/**
 * @file userspace/lib/widget/event.c
 * @brief Widget event distributor and handler
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
#include <stdio.h>

/**
 * @brief Find the best widget to match X/Y
 * @param frame The frame to check
 * @param x X coordinate (relative)
 * @param y Y coordinate (relative)
 * 
 * @warning This is a recursive method, ye be warned.
 */
static widget_t *widget_findBestWidget(widget_t *frame, int x, int y) {
    widget_t *best_match = frame;

    if (frame->children) {
        foreach(cn, frame->children) {
            widget_t *child = (widget_t*)cn->value;

            if (child->geometry) {
                int geom_x = 0;
                int geom_y = 0;
                widget_getCoordinates(child, &geom_x, &geom_y);

                if (x >= geom_x && x <= geom_x + (int)child->width && y >= geom_y && y <= geom_y + (int)child->height) {
                    best_match = child;
                }
                
            }
        }
    }

    if (best_match->type == WIDGET_TYPE_FRAME && best_match != frame) {
        // Get coordinates
        int geom_x = 0;
        int geom_y = 0;
        widget_getCoordinates(best_match, &geom_x, &geom_y);

        // Find the best widget
        return widget_findBestWidget(best_match, x - geom_x, y - geom_y);
    }

    return best_match;
}

/**
 * @brief Mouse callback
 */
static void widget_mouseCallback(window_t *win, uint32_t event_type, void *event) {
    widget_event_state_t *ev_state = ((widget_frame_t*)(((widget_t*)win->d)->impl))->event;

    if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN) {
        celestial_event_mouse_button_down_t *ev = (celestial_event_mouse_button_down_t*)event;
        
        // Find the best widget
        widget_t *w = widget_findBestWidget((widget_t*)win->d, ev->x, ev->y);

        // Get coordinates
        int geom_x = 0;
        int geom_y = 0;
        widget_getCoordinates(w, &geom_x, &geom_y);

        if (w->down) w->down(w, frame_getContext((widget_t*)win->d), ev->x - geom_x, ev->y - geom_y, ev->held);
    } else if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_UP) {
        celestial_event_mouse_button_up_t *ev = (celestial_event_mouse_button_up_t*)event;

        // Find the best widget
        widget_t *w = widget_findBestWidget((widget_t*)win->d, ev->x, ev->y);

        // Get coordinates
        int geom_x = 0;
        int geom_y = 0;
        widget_getCoordinates(w, &geom_x, &geom_y);

        if (w->up) w->up(w, frame_getContext((widget_t*)win->d), ev->x - geom_x, ev->y - geom_y, ev->released);
    } else if (event_type == CELESTIAL_EVENT_MOUSE_MOTION) {
        celestial_event_mouse_motion_t *ev = (celestial_event_mouse_motion_t*)event;

        // Find the best widget
        widget_t *w = widget_findBestWidget((widget_t*)win->d, ev->x, ev->y);

        // Get coordinates
        int geom_x = 0;
        int geom_y = 0;
        widget_getCoordinates(w, &geom_x, &geom_y);

        if (w == ev_state->held_widget) {
            // Send motion
            if (w->motion) w->motion(w, frame_getContext((widget_t*)win->d), ev->x - geom_x, ev->y - geom_y);
        } else {
            // We aren't the held widget
            if (ev_state->held_widget) {
                if (ev_state->held_widget->exit) ev_state->held_widget->exit(ev_state->held_widget, frame_getContext((widget_t*)win->d));
            }

            // Update held widget + send ENTER event
            ev_state->held_widget = w;
            if (w->enter) w->enter(w, frame_getContext((widget_t*)win->d), ev->x - geom_x, ev->y - geom_y);
        }
    }
}

/**
 * @brief Initialize events on a frame
 * @param frame The frame to init events on
 */
void widget_initEvents(struct widget *frame) {
    widget_frame_t *f = (widget_frame_t*)frame->impl;

    // Create event state
    f->event = malloc(sizeof(widget_event_state_t));
    f->event->frame = frame;
    f->event->held_widget = NULL;

    if (f->window) {
        // Register event handler callbacks
        celestial_setHandler(f->window, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, widget_mouseCallback);
        celestial_setHandler(f->window, CELESTIAL_EVENT_MOUSE_BUTTON_UP, widget_mouseCallback);
        celestial_setHandler(f->window, CELESTIAL_EVENT_MOUSE_MOTION, widget_mouseCallback);
    }
}