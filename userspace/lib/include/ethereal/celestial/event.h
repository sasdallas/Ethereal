/**
 * @file userspace/lib/include/ethereal/celestial/event.h
 * @brief Celestial events
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_EVENT_H
#define _CELESTIAL_EVENT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <ethereal/celestial/types.h>
#include <ethereal/celestial/window.h>
#include <kernel/fs/periphfs.h> // I hate myself

/**** DEFINITIONS ****/

#define CELESTIAL_MAGIC_EVENT                   0x41424344

/* Event types */
#define CELESTIAL_EVENT_MOUSE_ENTER             0x00000001
#define CELESTIAL_EVENT_MOUSE_MOTION            0x00000002
#define CELESTIAL_EVENT_MOUSE_BUTTON_DOWN       0x00000004
#define CELESTIAL_EVENT_MOUSE_BUTTON_UP         0x00000008
#define CELESTIAL_EVENT_MOUSE_DRAG              0x00000010  // Sent when window server detects that the mouse is moving and holding left
#define CELESTIAL_EVENT_MOUSE_EXIT              0x00000020
#define CELESTIAL_EVENT_FOCUSED                 0x00000040
#define CELESTIAL_EVENT_UNFOCUSED               0x00000080
#define CELESTIAL_EVENT_KEY_EVENT               0x00000100
#define CELESTIAL_EVENT_DEFAULT_SUBSCRIBED      0xFFFFFFFF

#define CELESTIAL_EVENT_COMMON                  uint32_t magic;\
                                                uint16_t type; \
                                                size_t size; \
                                                wid_t wid; \

/* Mouse */
#define CELESTIAL_MOUSE_BUTTON_LEFT             0x1
#define CELESTIAL_MOUSE_BUTTON_RIGHT            0x2
#define CELESTIAL_MOUSE_BUTTON_MIDDLE           0x4

/**** TYPES ****/

typedef struct celestial_event_header {
    CELESTIAL_EVENT_COMMON
} celestial_event_header_t;

typedef struct celestial_event_mouse_enter {
    CELESTIAL_EVENT_COMMON              // Common
    int32_t x;                          // Relative X position
    int32_t y;                          // Relative Y position
} celestial_event_mouse_enter_t;

typedef struct celestial_event_mouse_motion {
    CELESTIAL_EVENT_COMMON              // Common
    int32_t x;                          // Relative X position
    int32_t y;                          // Relative Y position
    int32_t buttons;                    // Held mouse buttons;
} celestial_event_mouse_motion_t;

typedef struct celesital_event_mouse_button_down {
    CELESTIAL_EVENT_COMMON              // Common
    int32_t x;                          // Relative X position
    int32_t y;                          // Relative Y position
    int32_t held;                       // The button that was held
} celestial_event_mouse_button_down_t;

typedef struct celesital_event_mouse_button_up {
    CELESTIAL_EVENT_COMMON              // Common
    int32_t x;                          // Relative X position
    int32_t y;                          // Relative Y position
    int32_t released;                   // The button that was released
} celestial_event_mouse_button_up_t;

typedef struct celestial_event_mouse_drag {
    CELESTIAL_EVENT_COMMON              // Common
    int32_t x;                          // Relative X position the mouse started dragging at
    int32_t y;                          // Relative Y position the mouse started dragging at
    int32_t win_x;                      // New window X
    int32_t win_y;                      // New window Y
} celestial_event_mouse_drag_t;

typedef struct celestial_event_mouse_exit {
    CELESTIAL_EVENT_COMMON              // Common
} celestial_event_mouse_exit_t;

typedef struct celestial_event_focused {
    CELESTIAL_EVENT_COMMON              // Common
} celestial_event_focused_t;

typedef struct celestial_event_unfocused {
    CELESTIAL_EVENT_COMMON              // Common
} celestial_event_unfocused_t;

typedef struct celestial_event_key {
    CELESTIAL_EVENT_COMMON              // Common
    key_event_t ev;                     // Key event
} celestial_event_key_t;


/**
 * @brief Celestial event handler
 * @param win The window the event occurred on
 * @param event_type The event which occurred
 * @param event The event object that occurred
 */
typedef void (*celestial_event_handler_t)(window_t *win, uint32_t event_type, void *event);


/**** FUNCTIONS ****/

/**
 * @brief Subscribe to specific events on the Celestial handler
 * @param win The window to subscribe to events on
 * @param events The events to subscribe to
 * @returns 0 on success
 */
int celestial_subscribe(window_t *win, uint32_t events);

/**
 * @brief Unsubscribe from specific events on the celestial handler
 * @param win The window to unsubscribe events on
 * @param events The events to unsubscribe from
 * @returns 0 on success
 */
int celestial_unsubscribe(window_t *win, uint32_t events);

/**
 * @brief Set a specific event handler in a window
 * @param win The window to set the handler for
 * @param event The event to set the handler for
 * @param handler The handler to set for the event
 * @returns 0 on success, -1 on failure
 */
int celestial_setHandler(window_t *win, uint32_t event, celestial_event_handler_t handler);

/**
 * @brief Handle a receieved event
 * @param event The event that was received
 */
void celestial_handleEvent(void *event);

#endif