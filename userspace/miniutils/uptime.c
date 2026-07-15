/**
 * @file userspace/miniutils/uptime.c
 * @brief Uptime utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void usage() {
    fprintf(stderr, "Usage: uptime [-rhV]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -r, --raw         show uptime as raw\n");
    fprintf(stderr, " -h, --help        display this message\n");
    fprintf(stderr, " -V, --version     output version information\n");
    exit(0);
}

void version() {
    printf("uptime (Ethereal miniutils) 1.10\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "raw", .has_arg = no_argument, .flag = NULL, .val = 'r' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };


    int index;
    int ch;
    int raw = 0;

    while ((ch = getopt_long(argc, argv, "rhV", (const struct option*)options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) {
            ch = options[index].val;
        }

        switch (ch) {
            case 'r':
                raw = 1;
                break;

            case 'V':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    FILE *f = fopen("/system/uptime", "r");
    if (!f) {
        perror("/system/uptime");
        return 1;
    }

    unsigned long seconds;
    unsigned long subseconds;
    fscanf(f, "%lu.%lu", &seconds, &subseconds);
    fclose(f);

    if (raw) {
        printf("%lu.%lu\n", seconds, subseconds);
        return 0;
    }
    
    // subseconds is useless to us then
    printf("up ");
    char *pre = "";
    if (seconds >= 60*60*24) {
        int days = seconds / (60 * 60 * 24);
        printf("%s%lu day%s", pre, days, days>1 ? "s" : "");
        seconds -= (days * (60 * 60 * 24));
        pre = ", ";
    }

    if (seconds >= 60*60) {
        int hours = seconds / (60 * 60);
        printf("%s%lu hour%s", pre, hours, hours>1 ? "s" : "");
        seconds -= (hours * (60 * 60));
        pre = ", ";
    }

    if (seconds >= 60) {
        int minutes = seconds / 60;
        printf("%s%lu minute%s", pre, minutes, minutes>1 ? "s" : "");
        seconds -= (minutes * (60));
        pre = ", ";
    }

    if (seconds) {
        printf("%s%lu second%s", pre, seconds, seconds>1 ? "s" : "");
    }

    printf("\n");

    return 0;
}
