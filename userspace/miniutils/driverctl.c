/**
 * @file userspace/miniutils/driverctl.c
 * @brief Load/unload/check drivers (insmod equivalent)
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
#include <sys/ethereal/driver.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

void usage() {
    printf("Usage: driverctl [-l FILE] [-u ID] [-q ID]\n");
    printf("Ethereal driver manager\n");
    printf(" -l, --load         Load a driver\n");
    printf(" -u, --unload       Unload a driver by its ID\n");
    printf(" -q, --query        Query a driver by its ID\n");
    printf(" -h, --help         Display this help message\n");
    exit(1);
}

void version() {
    printf("driverctl version 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    const struct option options[] = {
        { .name = "load", .has_arg = required_argument, .flag = NULL, .val = 'l' },
        { .name = "unload", .has_arg = required_argument, .flag = NULL, .val = 'u' },
        { .name = "query", .has_arg = required_argument, .flag = NULL, .val = 'q' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
    };

    int longidx;
    int ch;

    opterr = 1;
    while ((ch = getopt_long(argc, argv, "l:u:q:hv", options, &longidx)) != -1) {
        if (!ch && options[longidx].flag == NULL) ch = options[longidx].val;

        switch (ch) {
            case 'l':
                if (geteuid()) {
                    fprintf(stderr, "driverctl: Only root can load drivers.\n");
                    return 1;
                }

                printf("Loading driver: %s\n", optarg);

                pid_t status = ethereal_loadDriver(optarg, DRIVER_IGNORE, &argv[optind-1]); 
                if (status < 0) {
                    fprintf(stderr, "\033[0;31mLoading driver '%s' failed: %s\n\033[0m", optarg, strerror(errno));
                    return 1;
                }

                printf("\033[0;32mDriver successfully loaded with ID %d\n\033[0m", status);
                return 0;

            case 'u':
                if (geteuid()) {
                    fprintf(stderr, "driverctl: Only root can unload drivers.\n");
                    return 1;
                }

                long id = strtol(optarg, NULL, 10);
                printf("Unloading driver: %d\n", id);

                if (ethereal_unloadDriver(id) < 0) {
                    fprintf(stderr, "\033[0;31mUnloading driver %d failed: %s\n\033[0m", id, strerror(errno));
                    return 1;
                } 

                printf("\033[0;32mDriver successfully unloaded\n\033[0m");
                return 0;

            case 'q':
                if (geteuid()) {
                    fprintf(stderr, "driverctl: Only root can query drivers.\n");
                    return 1;
                }

                long did = strtol(optarg, NULL, 10);
                ethereal_driver_t *d = ethereal_getDriver(did);

                if (!d) {
                    fprintf(stderr, "\033[0;31mQuerying driver %d failed: %s\n\033[0m", did, strerror(errno));
                    return 1;
                }

                printf("\033[0;32mFilename:\033[0m %s\n", d->filename);
                printf("\033[0;33mDriver name:\033[0m %s\n", d->metadata.name);
                if (strlen(d->metadata.author)) {
                    printf("\033[0;34mAuthor:\033[0m %s\n", d->metadata.author);
                }
                printf("\033[0;33mLoad range:\033[0m %p - %p\n", d->base, d->base + d->size);

                return 0;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage(); 
                break;
        }
    }

    fprintf(stderr, "driverctl: You must specify an operation to perform\n");
    fprintf(stderr, "Try 'driverctl --help' for more information.\n");
    return 1;
}