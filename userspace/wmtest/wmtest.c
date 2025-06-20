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
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <ethereal/celestial.h>

void mouse_handler(window_t *win, uint32_t event_type, void *event) {
    switch (event_type) {
        case CELESTIAL_EVENT_MOUSE_ENTER:
            fprintf(stderr, "mouse enter\n");
            return;
        case CELESTIAL_EVENT_MOUSE_EXIT:
            fprintf(stderr, "mouse exit\n");
            return;
        default:
            fprintf(stderr, "unknown event\n");
            return;
    }
}

int main(int argc, char *argv[]) {

    wid_t wid = celestial_createWindowUndecorated(0, 256, 256);
    fprintf(stderr, "got wid: %d\n", wid);

    window_t *win = celestial_getWindow(wid);
    fprintf(stderr, "window %d: %dx%d at X %d Y %d\n", wid, win->width, win->height, win->x, win->y);

    uint32_t *fb = celestial_getFramebuffer(win);
    memset(fb, 0xFF, win->width * win->height * 4);    

    fprintf(stderr, "subscribe to mouse event");
    if (celestial_subscribe(win, CELESTIAL_EVENT_MOUSE_ENTER | CELESTIAL_EVENT_MOUSE_EXIT) != 0) {
        fprintf(stderr, "subscribe failed\n");
        return 1;
    }

    celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_ENTER, mouse_handler);
    celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_EXIT, mouse_handler);

    celestial_mainLoop();
    for (;;);
    return 0;
}