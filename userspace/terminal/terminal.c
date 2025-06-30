/**
 * @file userspace/terminal/terminal.c
 * @brief Tiny terminal emulator for Ethereal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ethereal/keyboard.h>
#include <pty.h>
#include <poll.h>

int main(int argc, char *argv[]) {
    printf("Starting Ethereal terminal...\n");

    // Make keyboard state
    keyboard_t *kbd = keyboard_create();
    int kbfd = open("/device/keyboard", O_RDONLY);

    if (kbfd < 0) {
        perror("open");
        return 1;
    }

    // Get a PTY
    int amaster, aslave;
    if (openpty(&amaster, &aslave, NULL, NULL, NULL) < 0) {
        perror("openpty");
        return 1;
    }

    pid_t cpid = fork();
    if (!cpid) {
        setsid();
        dup2(aslave, STDIN_FILENO);
        dup2(aslave, STDOUT_FILENO);
        dup2(aslave, STDERR_FILENO);
        ioctl(aslave, TIOCSCTTY, 1);
        tcsetpgrp(aslave, getpid());

        char *nargv[] = { "essence", NULL };
        execvp("essence", (const char**)nargv);
        exit(1);
    } else {
        while (1) {
            // Enter waiting loop
            struct pollfd fds[] = { { .fd = amaster, .events = POLLIN, .revents = 0 }, { .fd = kbfd, .events = POLLIN, .revents = 0 }};
            int p = poll(fds, 2, -1);

            if (p < 0) {
                perror("poll");
                exit(1);
            }

            if (fds[0].revents & POLLIN) {
                // We have data on pty
                char buf[4096];
                ssize_t r = read(amaster, buf, 4096);
                if (r) {
                    buf[r] = 0;

                    fprintf(stdout, "%s", buf);
                    fflush(stdout);
                }
            }
            
            if (fds[1].revents & POLLIN) {
                // We have key events
                key_event_t evp;
                ssize_t r = read(kbfd, &evp, sizeof(key_event_t));
                if (r != sizeof(key_event_t)) continue;

                keyboard_event_t *ev = keyboard_event(kbd, &evp);
                
                if (ev->type == KEYBOARD_EVENT_PRESS && ev->ascii) {
                    char b[] = { ev->ascii };
                    write(amaster, b, 1);
                }
                 
                free(ev);
            }

        }
    }
}