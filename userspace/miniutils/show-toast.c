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

int main(int argc, char *argv[]) {
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


    toast_t t = { 
        .flags = 0,
        .icon = "/usr/share/icons/16/Ethereal.bmp",
        .title = "Title",
        .description = "Description\nWITH a newline!"
    };

    send(sock, &t, sizeof(toast_t), 0);

    close(sock);

    printf("Toast sent\n");
    return 0;
}