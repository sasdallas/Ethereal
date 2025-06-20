/**
 * @file userspace/celestial/event.c
 * @brief Event logic
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"

/**
 * @brief Send an event to a specific window
 * @param win The window to send the event to
 * @param event The event type to send
 * @returns 0 on success
 */
int event_send(wm_window_t *win, void *event) {
    // Check to make sure the event is subscribed to
    celestial_event_header_t *hdr = (celestial_event_header_t*)event;
    CELESTIAL_DEBUG("event: Send event %d\n", hdr->type);
    if (!((win->events & hdr->type) == hdr->type)) return 0; // Not subscribed
    return socket_send(win->sock, hdr->size, event);
}
