/**
 * @file userspace/test2/wmtest.c
 * @brief Test for celestial
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <ethereal/keyboard.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

#include <ethereal/celestial.h>
#include <ethereal/widget.h>


int pty_master, pty_slave;
keyboard_t *kbd = NULL;

void mouse_handler(window_t *win, uint32_t event_type, void *event) {
    switch (event_type) {
        case CELESTIAL_EVENT_MOUSE_ENTER:
            fprintf(stderr, "mouse enter\n");
            return;
        case CELESTIAL_EVENT_MOUSE_EXIT:
            fprintf(stderr, "mouse exit\n");
            return;
        case CELESTIAL_EVENT_MOUSE_MOTION:
            celestial_event_mouse_motion_t *motion = (celestial_event_mouse_motion_t*)event; 
            fprintf(stderr, "motion: %d %d\n", motion->x, motion->y);
            return;
        default:
            fprintf(stderr, "unknown event\n");
            return;
    }
}

void kbd_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_key_t *key = (celestial_event_key_t*)event;
    keyboard_event_t *ev = keyboard_event(kbd, &key->ev);
                
    if (ev->type == KEYBOARD_EVENT_PRESS && ev->ascii) {
        if (ev->ascii == '\b') ev->ascii = 0x7F;
        char b[] = { ev->ascii };
        write(pty_master, b, 1);
    }
}

void btn1_click(widget_t *widget, void *d) {
    printf("button 1 clicked: data=%p\n", d);
}

void btn2_click(widget_t *widget, void *d) {
    printf("button 2 clicked: data=%p\n", d);
}

int main(int argc, char *argv[]) {
    wid_t wid = celestial_createWindow(0, 512, 256);
    fprintf(stderr, "got wid: %d\n", wid);

    window_t *win = celestial_getWindow(wid);
    fprintf(stderr, "window %d: %dx%d at X %d Y %d\n", wid, win->width, win->height, win->x, win->y);
    
    widget_t *frame = frame_createRoot(win, FRAME_DEFAULT);
    widget_t *lbl = label_create(frame, "Hello, widgets!", 12);
    widget_renderAtCoordinates(lbl, 30, 30);

    widget_t *btn = button_create(frame, "OK", GFX_RGB(0, 0, 0), BUTTON_ENABLED);
    widget_renderAtCoordinates(btn, 100, 100);
    widget_setHandler(btn, WIDGET_EVENT_CLICK, btn1_click, (void*)0xDEADDEAD);

    // widget_t *btn2 = button_create(frame, "This is a button", GFX_RGB(0, 0, 0), BUTTON_ENABLED);
    // widget_renderAtCoordinates(btn2, 300, 150);
    // widget_setHandler(btn2, WIDGET_EVENT_CLICK, btn2_click, (void*)0xCAFECAFE);

    widget_t *btn3 = button_create(frame, "Big button", GFX_RGB(0,0,0), BUTTON_ENABLED);
    btn3->width = 100;
    btn3->height = 100;

    widget_renderAtCoordinates(btn3, 300, 150);


    widget_render(celestial_getGraphicsContext(win), frame);
    
    gfx_render(celestial_getGraphicsContext(win));
    celestial_flip(win);

    

    celestial_mainLoop();

    return 0;
}