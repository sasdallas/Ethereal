/**
 * @file userspace/miniutils/show-toast.c
 * @brief Show toast utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ethereal/toast.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdlib.h>

char *title = "Title";
char *description = "Description";
char *icon = "/usr/share/icons/16/Ethereal.bmp";

enum {
    OPT_TEXT, 
    OPT_TITLE,
    OPT_ICON,
    OPT_HELP,
    OPT_VERSION
};

void usage() {
    fprintf(stderr, "Usage: show-toast [OPTIONS]\n");
    fprintf(stderr, "Show a toast of your choosing\n");
    fprintf(stderr, " --text=TEXT           Set the text of the toast\n");
    fprintf(stderr, " --title=TITLE         Set the title of the toast\n");
    fprintf(stderr, " --icon=ICON           Set the icon of the toast\n");
    fprintf(stderr, " --help                Show this help message\n");
    fprintf(stderr, " --version             Print the version of show-dialog\n");

    exit(1);
}

void version() {
    printf("show-toast version 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {

    const struct option options[] = {
        { .name = "text", .flag = NULL, .has_arg = required_argument, .val = OPT_TEXT, },
        { .name = "title", .flag = NULL, .has_arg = required_argument, .val = OPT_TITLE, },
        { .name = "icon", .flag = NULL, .has_arg = required_argument, .val = OPT_ICON, },
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = OPT_HELP, },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = OPT_VERSION, },
        { .name = NULL, .flag = NULL, .has_arg = no_argument, .val = 0 },
    };

    int ch;
    int index;

    while ((ch = getopt_long(argc, argv, "", options, &index)) != -1) {
        if (!ch) ch = options[index].val;

        switch (ch) {
            case OPT_TEXT:
                description = strdup(optarg);
                break;

            case OPT_TITLE:
                title = strdup(optarg);
                break;

            case OPT_ICON:
                icon = strdup(optarg);
                break;

            case OPT_VERSION:
                version();
                break;

            default:
                usage();
        }
    }



    struct stat st;
    if (stat("/comm/toast-server", &st) < 0) {
        fprintf(stderr, "\033[0;31mError connecting to toast server:\033[0m %s\n", strerror(errno));
        return 1;
    }
    
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un un = { .sun_family = AF_UNIX, .sun_path = "/comm/toast-server" };
    if (connect(sock, (const struct sockaddr*)&un, sizeof(struct sockaddr_un)) < 0) {
        perror("connect");
        return 1;
    }


    toast_t t;
    t.flags = 0;
    strncpy(t.title, title, 128);
    strncpy(t.description, description, 128);
    strncpy(t.icon, icon, 64);

    send(sock, &t, sizeof(toast_t), 0);

    close(sock);

    return 0;
}