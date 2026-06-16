/**
 * @file userspace/celestial/event.h
 * @brief Event system
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_EVENT_H
#define _CELESTIAL_WM_EVENT_H

/**** INCLUDES ****/
#include <ethereal/celestial/types.h>
#include <ethereal/celestial/event.h>
#include "window.h"

/**** FUNCTIONS ****/

/**
 * @brief Send an event to a specific window
 * @param win The window to send the event to
 * @param event The event type to send
 * @returns 0 on success
 */
int event_send(wm_window_t *win, void *event);

#endif