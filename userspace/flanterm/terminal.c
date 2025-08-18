/**
 * @file userspace/flanterm/terminal.c
 * @brief Flanterm terminal emulator for Ethereal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ethereal/keyboard.h>
#include <pty.h>
#include <poll.h>
#include <graphics/gfx.h>
#include <ethereal/ansi.h>
#include <getopt.h>

/* Flanterm */
struct flanterm_context *ft_ctx;

/* Keyboard */
keyboard_t *kbd = NULL;

/* PTY */
int pty_master;
int pty_slave;

void kbd_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_key_t *key = (celestial_event_key_t*)event;
    keyboard_event_t *ev = keyboard_event(kbd, &key->ev);
                
    if (ev->type == KEYBOARD_EVENT_PRESS) {
        if (ev->ascii == '\b') ev->ascii = 0x7F;
        
        if (!ev->ascii) return;
        char b[] = {ev->ascii};
        write(pty_master, b, 1);
    }
}

int main(int argc, char *argv[]) {
    wid_t wid = celestial_createWindow(0, 640, 480);
    window_t *win = celestial_getWindow(wid);
    celestial_setHandler(win, CELESTIAL_EVENT_KEY_EVENT, kbd_handler);

    gfx_context_t *ctx = celestial_getGraphicsContext(win);

    ft_ctx = flanterm_fb_init(
        NULL,NULL,
        ctx->backbuffer,
        ctx->width,
        ctx->height,
        ctx->pitch,
        8,16,8,8,8,0,
        NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, 0, 0, 1, 0, 0, 0
    );

    // Create keyboard fd
    int keyboard_fd = open("/device/keyboard", O_RDONLY);

    if (keyboard_fd < 0) {
        perror("open");
        return 1;
    }

    
    // Create keyboard object
    kbd = keyboard_create();

    // Create the PTY
    if (openpty(&pty_master, &pty_slave, NULL, NULL, NULL) < 0) {
        perror("openpty");
        exit(1);
    }

    pid_t cpid = fork();
    if (!cpid) {
        // Spawn startup program
        setsid();

        dup2(pty_slave, STDIN_FILENO);
        dup2(pty_slave, STDOUT_FILENO);
        dup2(pty_slave, STDERR_FILENO);

        ioctl(pty_slave, TIOCSCTTY, 1);
        tcsetpgrp(pty_slave, getpid());

        // Spawn shell
        // TODO: Support argc/argv
        char *argv[] = { "essence", NULL };
        execvp("essence", (const char **)argv);
        exit(1);
    }

    // dup2(pty_master, STDIN_FILENO);
    // dup2(pty_master, STDOUT_FILENO);

    // Enter main loop
    for (;;) {
        // Get events
        struct pollfd fds[] = {{ .fd = pty_master, .events = POLLIN }, { .fd = celestial_getSocketFile(), .events = POLLIN }};
        int p = poll(fds, 2, -1);
        if (p < 0) return 1;
        if (!p) continue;


        if (fds[0].revents & POLLIN) {
            // We have data on PTY
            char buf[4096];
            ssize_t r = read(pty_master, buf, 4096);
            if (r) {
                buf[r] = 0;

                flanterm_write(ft_ctx, buf, r);

                gfx_render(ctx);
                celestial_flip(win);
            }
        }

        if (fds[1].revents & POLLIN) {
            celestial_poll();
        }
    }

    return 0;


}
