/**
 * @file userspace/miniutils/mount.c
 * @brief mount equivalent
 * 
 * @todo Handle data and flags
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

void usage() {
    printf("Usage: mount [-t TYPE] [DEVICE] [MOUNTPOINT]\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("mount (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int c;
    int index;

    // Mount type
    char *type = NULL; 

    struct option options[] = {
        { .name = "type", .has_arg = required_argument, .flag = NULL, .val = 't' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    while ((c = getopt_long(argc, argv, "t:hv", (const struct option*)&options, &index)) != -1) {
        if (!c && options[index].flag == 0) {
            c = options[index].val;
        }

        switch (c) {
            case 't':
                type = strdup(optarg);
                break;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    if (argc-optind < 2) usage();

    char *device = argv[optind];
    char *mountpoint = argv[optind+1];

    if (mount(device, mountpoint, type, 0, NULL) < 0) {
        perror("mount");
        return 1;
    }

    return 0;
}