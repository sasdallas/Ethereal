/**
 * @file userspace/miniutils/sleep.c
 * @brief Sleep utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void usage() {
    printf("Usage: sleep NUMBER[SUFFIX]\n");
    printf("SUFFIX may be 's','m','h', or 'd', for seconds, minutes, hours, days.\n");
    exit(0);
}

void version() {
    printf("sleep (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    const struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v' },
        { .name = NULL, .flag = NULL, .has_arg = no_argument, .val  = 0 },
    };

    int ch;
    int index;

    while ((ch = getopt_long(argc, argv, "hv", options, &index)) != -1) {
        if (!ch) ch = options[index].val;

        if (ch == 'v') {
            version();
        } else {
            usage();
        }
    }

    if (!(argc-optind)) usage();
    char *arg = argv[optind];

    char *end = NULL;
    long num = strtol(arg, &end, 10);

    int mult = 1;

    if (end) {
        if (*end == 'm') {
            mult = 60;
        } else if (*end == 'h') {
            mult = 60 * 60;
        } else if (*end == 'd') {
            mult = 60 * 60 * 24;
        } else if (*end) {
            fprintf(stderr, "sleep: invalid time interval \'%s\'\n", arg);
            fprintf(stderr, "Try 'sleep --help' for more information\n");
            return 1;
        }
    }

    sleep(num * mult);
    return 0;
}