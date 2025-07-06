/**
 * @file userspace/miniutils/uptime.c
 * @brief Print uptime
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
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *f = fopen("/kernel/uptime", "r");
    if (!f) return 1;

    char buf[1024];
    fgets(buf, 1024, f);
    *(strchrnul(buf, '.')) = 0;

    long seconds = strtol(buf, NULL, 10);

    printf("up ");

    // Depending on whether we have more than a day...
    if (seconds > (24 * (60 * 60))) {
        int days = seconds / (24 * (60 * 60));
        seconds -= (24 * (60 * 60)) * days;
        printf("%d day%s ", days, days != 1 ? "s" : "");
    }

    // More than an hour...
    if (seconds > (60 * 60)) {
        int hours = seconds / (60 * 60);
        seconds -= hours * (60 * 60);
        printf("%d hour%s ", hours, hours != 1 ? "s" : "");
    }

    // More than a minute...
    if (seconds > (60)) {
        int minutes = seconds / 60;
        seconds -= minutes * (60);
        printf("%d minute%s ", minutes, minutes != 1 ? "s" : "");
    }

    printf("%d second%s\n", seconds, seconds != 1 ? "s" : "");
    return 0;
}