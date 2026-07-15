/**
 * @file celestial/backend/input_linux.c
 * @brief Input daemon
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifdef BUILDING_LINUX

#include "../celestial.h"
#include <ethereal/celestial.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <unistd.h>


void mouse_process_motion(XMotionEvent *event) {
    // TRACE_DEBUG("mouse_process_motion %d %d\n", event->x, event->y);
    mouse_check_events(event->x, event->y, 0, SERVER->last_buttons);
}


void mouse_process_press(XButtonEvent *event) {
    uint32_t new_buttons = SERVER->last_buttons;

    if (event->button == Button1) {
        new_buttons |= (MOUSE_BUTTON_LEFT);
    } else if (event->button == Button2) {
        new_buttons |= (MOUSE_BUTTON_RIGHT); 
    } else if (event->button == Button3) {
        new_buttons |= (MOUSE_BUTTON_MIDDLE); 
    } else {
        return;
    }
    

    mouse_check_events(event->x, event->y, 0, new_buttons); 
}

void mouse_process_release(XButtonEvent *event) {
    uint32_t new_buttons = SERVER->last_buttons;

    if (event->button == Button1) {
        new_buttons &= ~(MOUSE_BUTTON_LEFT);
    } else if (event->button == Button2) {
        new_buttons &= ~(MOUSE_BUTTON_RIGHT); 
    } else if (event->button == Button3) {
        new_buttons &= ~(MOUSE_BUTTON_MIDDLE); 
    } else {
        return;
    }
    

    mouse_check_events(event->x, event->y, 0, new_buttons); 
}


int input_init_backend() {
    return 0;
}

#endif