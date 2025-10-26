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
#include <sys/ioctl.h>
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

static void terminal_sendInput(char *input) {
    write(pty_master, input, strlen(input));
}

void term_write(keyboard_event_t *event) {
    switch (event->scancode) {
        case SCANCODE_UP_ARROW:
            terminal_sendInput("\033[A");
            break;

        case SCANCODE_DOWN_ARROW:
            terminal_sendInput("\033[B");
            break;

        case SCANCODE_RIGHT_ARROW:
            terminal_sendInput("\033[C");
            break;


        case SCANCODE_LEFT_ARROW:
            terminal_sendInput("\033[D");
            break;


        default:
            if (!event->ascii) return;
            char b[] = {event->ascii};
            write(pty_master, b, 1);
            break;
    } 
}

void kbd_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_key_t *key = (celestial_event_key_t*)event;
    keyboard_event_t *ev = keyboard_event(kbd, &key->ev);
                
    if (ev->type == KEYBOARD_EVENT_PRESS) {
        if (ev->ascii == '\b') ev->ascii = 0x7F;
        
        term_write(ev);
        free(ev);
    }
}

#define TERM_WIDTH          8*80
#define TERM_HEIGHT         16*24

int main(int argc, char *argv[]) {
    gfx_context_t *ctx = NULL;
    window_t *win = NULL;

    if (argc < 2 || strcmp(argv[1], "-f")) {
        wid_t wid = celestial_createWindow(0, TERM_WIDTH, TERM_HEIGHT);
        win = celestial_getWindow(wid);
        celestial_setHandler(win, CELESTIAL_EVENT_KEY_EVENT, kbd_handler);
        ctx = celestial_getGraphicsContext(win);
    } else {
        ctx = gfx_createFullscreen(CTX_DEFAULT);
    }

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



    size_t cols, rows;
    flanterm_get_dimensions(ft_ctx, &cols, &rows);

    struct winsize size = {
        .ws_col = cols,
        .ws_row = rows,
    };;

    // Create the PTY
    if (openpty(&pty_master, &pty_slave, NULL, NULL, &size) < 0) {
        perror("openpty");
        exit(1);
    }

    struct termios attr;
    tcgetattr(pty_master, &attr);
    attr.c_iflag |= INLCR;
    attr.c_iflag &= ~(ICRNL);

    tcsetattr(pty_master, TCSANOW, &attr);

    // Temporary for ncurses
    putenv("TERM=vt220");

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
        execvp("essence", argv);
        exit(1);
    }

    // dup2(pty_master, STDIN_FILENO);
    // dup2(pty_master, STDOUT_FILENO);

    // Enter main loop
    for (;;) {
        // Get events
        struct pollfd fds[] = { { .fd = keyboard_fd, .events = POLLIN }, { .fd = pty_master, .events = POLLIN }, { .fd = celestial_getSocketFile(), .events = POLLIN }};
        int p = poll(fds, 3, -1);
        if (p < 0) return 1;
        if (!p) continue;

        if (fds[0].revents & POLLIN && !win) {
            key_event_t evp;
            ssize_t r = read(keyboard_fd, &evp, sizeof(key_event_t));
            if (r != sizeof(key_event_t)) continue;

            keyboard_event_t *ev = keyboard_event(kbd, &evp);
            
            if (ev && ev->type == KEYBOARD_EVENT_PRESS) {
                if (ev->ascii == '\b') ev->ascii = 0x7f;
                term_write(ev);
                
            }

            if (ev) free(ev);
        }

        if (fds[1].revents & POLLIN) {
            // We have data on PTY
            char buf[4096];
            ssize_t r = read(pty_master, buf, 4096);
            if (r) {
                buf[r] = 0;

                flanterm_write(ft_ctx, buf, r);

                gfx_render(ctx);
                if (win) celestial_flip(win);
            }
        }

        if (win && fds[2].revents & POLLIN) {
            celestial_poll();
        }
    }

    return 0;


}
