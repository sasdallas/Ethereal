/**
 * @file userspace/miniutils/bananchat.c
 * @brief Connect to bananymous' chat server
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#define DEFAULT_SERVER_IP "chat.bananymous.com"
#define DEFAULT_SERVER_PORT 12345

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: bananchat <USERNAME>\n");
        return 1;
    }

    printf("Establishing connection to %s:%d\n", DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT);

    struct hostent *ent = gethostbyname(DEFAULT_SERVER_IP);
    if (!ent) {
        printf("dns-resolve: %s: not found by DNS\n", DEFAULT_SERVER_IP);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // Bind to port 1234
    struct sockaddr_in src = {
        .sin_family = AF_INET,
        .sin_port = htons(1234),
        .sin_addr.s_addr = 0,
    };

    if (bind(sock, (const struct sockaddr*)&src, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return 1;
    } 

    struct sockaddr_in dest = {
        .sin_port = htons(DEFAULT_SERVER_PORT),
        .sin_family = AF_INET,
    };

    memcpy(&dest.sin_addr.s_addr, ent->h_addr_list[0], ent->h_length);

    if (connect(sock, (const struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to banan-chat\n");

    // Send initial username
    if (send(sock, argv[1], strlen(argv[1]), 0) < 0) {
        perror("send");
        return 1;
    }

    // Enter main loop
    printf("Listening for new messages (no, you can't type sorry)\n");
    while (1) {
        struct pollfd fds[1];
        fds[0].fd = sock;
        fds[0].events = POLLIN;

        int p = poll(fds, 1, 0);
        if (p < 0) {
            perror("poll");
            return 1;
        }

        if (!p) continue;

        // Check for events on socket
        if (p && fds[0].revents & POLLIN) {
            // We have socket events
            char data[4096];
            
            ssize_t r = 0;
            if ((r = recv(sock, data, 4096, 0)) < 0) {
                perror("recv");
                return 1;
            } 

            data[r] = 0;
            printf("%s", data);
            fflush(stdout);
        }
    }
}