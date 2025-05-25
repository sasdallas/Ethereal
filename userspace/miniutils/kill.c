/**
 * @file userspace/miniutils/kill.c
 * @brief kill
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void usage() {
    printf("Usage: kill [OPTIONS] <pid>\n");
    printf("Forcibly terminate a process.\n");
    exit(EXIT_SUCCESS);
}

void version() {
    printf("kill (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
    }

    if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
        version();
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        usage();
    }

    // TODO: Other signal support

    // TODO: Burn this
    char *pid_str = argv[1];
    pid_t pid = strtol(pid_str, NULL, 10);
    if (pid || *(argv[1]) == '0') {
        if (kill(pid, SIGTERM) < 0) {
            printf("kill: %d: %s\n", pid, strerror(errno)); 
            return 1;
        }
    }

    return 0;
}