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


void background() {
    wid_t bgwid = celestial_createWindowUndecorated(0, 1024, 768);
    window_t *win = celestial_getWindow(bgwid);
    celestial_setZArray(win, CELESTIAL_Z_BACKGROUND);
    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    celestial_unsubscribe(win, CELESTIAL_EVENT_DEFAULT_SUBSCRIBED);

    FILE *bg = fopen("/usr/share/lines.bmp", "r");
    sprite_t *sp = gfx_createSprite(0,0);
    gfx_loadSprite(sp, bg);
    gfx_renderSprite(ctx, sp, 0, 0);
    gfx_font_t *font = gfx_loadFont(ctx, "/usr/share/UbuntuMono-Regular.ttf");
    gfx_renderString(ctx, font, "Ethereal x86_64", 10, 10, GFX_RGB(255, 255, 255));
    
    struct utsname un;
    uname(&un);

    char str[256];
    snprintf(str, 256, "%s %s %s", un.sysname, un.release, un.version);

    gfx_renderString(ctx, font, str, 10, 30, GFX_RGB(255, 255, 255));

    gfx_render(ctx);

    celestial_flip(win);
}

int main(int argc, char *argv[]) {
    background();
    
    wid_t wid = celestial_createWindow(0, 512, 256);
    fprintf(stderr, "got wid: %d\n", wid);

    window_t *win = celestial_getWindow(wid);
    fprintf(stderr, "window %d: %dx%d at X %d Y %d\n", wid, win->width, win->height, win->x, win->y);
    

    fprintf(stderr, "subscribe to mouse event");
    if (celestial_subscribe(win, CELESTIAL_EVENT_MOUSE_ENTER | CELESTIAL_EVENT_MOUSE_EXIT) != 0) {
        fprintf(stderr, "subscribe failed: %s\n", strerror(errno));
        return 1;
    }
    fprintf(stderr, "Subscribe OK\n");

    celestial_setTitle(win, "Celestial Demonstration Window");
    // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_ENTER, mouse_handler);
    // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_EXIT, mouse_handler);
    // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_MOTION, mouse_handler);
    // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_BUTTON_UP, mouse_handler);
    celestial_setHandler(win, CELESTIAL_EVENT_KEY_EVENT, kbd_handler);
    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    gfx_clear(ctx, GFX_RGB(0, 0, 0));
    gfx_render(ctx);
    gfx_font_t *font = gfx_loadFont(ctx, "/usr/share/UbuntuMono-Regular.ttf");
    celestial_flip(win);

    // Create a PTY
    openpty(&pty_master, &pty_slave, NULL, NULL, NULL);

    int x = 0;
    int y = 40;
    
    kbd = keyboard_create();

    // Fork
    pid_t cpid = fork();
    if (!cpid) {
        dup2(pty_slave, STDIN_FILENO);
        dup2(pty_slave, STDOUT_FILENO);
        dup2(pty_slave, STDERR_FILENO);

        setsid();
        ioctl(STDIN_FILENO, TIOCSCTTY, 1);
        tcsetpgrp(STDIN_FILENO, getpid());

        char *nargv[] = { "essence", NULL };
        execvp("essence", (const char**)nargv);
        exit(1);
    } else {
        for (;;) {
            celestial_poll();

            struct pollfd fds[] = { { .fd = pty_master, .events = POLLIN } };
            int p = poll(fds, 1, 0);
            if (!p) continue;
            
            char buf[4096];
            ssize_t r = read(pty_master, buf, 4096);
            if (r <= 0) continue;
            buf[r] = 0;

            for (int i = 0; i < r; i++) {
                if (x >= (int)ctx->width) {
                    x = 0;
                    y += 10;
                }


                if (y >= (int)ctx->height) {
                    y = 10;
                    x = 0;
                    gfx_clear(ctx, GFX_RGB(0,0,0));
                    gfx_render(ctx);
                    celestial_flip(win);
                }

                if (buf[i] == '\n') {
                    x = 0;
                    y += 10;
                    continue;
                }

                if (!buf[i]) continue;

                gfx_renderCharacter(ctx, font, buf[i], x, y, GFX_RGB(255,255,255));
                x += gfx_getAdvanceX(ctx, font, buf[i]);
                // x += 10;
            }


            gfx_render(ctx);
            celestial_flip(win);
        }
    }


    return 0;
}