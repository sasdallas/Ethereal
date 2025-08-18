/**
 * @file userspace/miniutils/hostname.c
 * @brief hostname utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

void usage() {
    printf("Usage: hostname [OPTION..] [NAME]\n");
    printf("Show or set the system's host name.\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("hostname (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    struct option options[] = {
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h', },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
    };

    int optindex;
    int ch;

    while ((ch = getopt_long(argc, argv, "hv", (const struct option*)&options, &optindex)) != -1) {
        if (!ch && options[optindex].flag == NULL) ch = options[optindex].val;

        switch (ch) {
            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    } 

    if (!(argc-optind)) {
        char name[256] = { 0 };
        if (gethostname(name, 256) < 0) {
            fprintf(stderr, "hostname: gethostname: %s\n", strerror(errno));
            return 1;
        }

        printf("%s\n", name);
    } else {
        if (sethostname(argv[optind], strlen(argv[optind])) < 0) {
            fprintf(stderr, "hostname: sethostname: %s\n", strerror(errno));
            return 1;
        }
    }

    return 0;
}