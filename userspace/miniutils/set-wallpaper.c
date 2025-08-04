/**
 * @file userspace/miniutils/set-wallpaper.c
 * @brief Set wallpaper utility
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // TODO: More arguments?
    if (argc < 2) {
        fprintf(stderr, "Usage: set-wallpaper [WALLPAPER]\n");
        return 1;
    }

    // Validate wallpaper file exists
    struct stat st;
    if (stat(argv[1], &st) < 0) {
        fprintf(stderr, "set-wallpaper: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    // Modify /tmp/wallpaper
    FILE *wp_file = fopen("/tmp/wallpaper", "w+");

    if (!wp_file) {
        fprintf(stderr, "set-wallpaper: Error opening /tmp/wallpaper: %s\n", strerror(errno));
        return 1;
    }

    if (fwrite(argv[1], strlen(argv[1]), 1, wp_file) != 1) {
        fprintf(stderr, "set-wallpaper: Error writing to /tmp/wallpaper: %s\n", strerror(errno));
        return 1;
    }

    fclose(wp_file);

    // To make it easier on us, the desktop server has very thoughtfully exposed its PID in /comm/desktop.pid

    FILE *desktop_pid = fopen("/comm/desktop.pid", "r");
    if (!desktop_pid) {
        fprintf(stderr, "set-wallpaper: /comm/desktop.pid: %s\n", strerror(errno));
        return 1;
    }

    char pid_buffer[128] = { 0 };
    fread(pid_buffer, 128, 1, desktop_pid);
    fclose(desktop_pid);

    pid_t p = strtol(pid_buffer, NULL, 10);
    
    // Send refresh signal
    if (kill(p, SIGUSR2)) {
        fprintf(stderr, "set-wallpaper: Error sending SIGUSR2: %s\n", strerror(errno));
        return 1;
    }

    printf("\033[0;32mWallpaper updated successfully\n\033[0m");
    return 0;
}