/**
 * @file userspace/lib/include/ethereal/widget/event.h
 * @brief Widget event system
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_EVENT_H
#define _WIDGET_EVENT_H

/**** INCLUDES ****/

#include <ethereal/celestial.h>

/**** TYPES ****/

struct widget;

typedef struct widget_event_state {
    struct widget *frame;               // Parent frame of this event state
    struct widget *held_widget;         // The currently held widget
} widget_event_state_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize events on a frame
 * @param frame The frame to init events on
 */
void widget_initEvents(struct widget *frame);

#endif