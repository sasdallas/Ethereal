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
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define DEFAULT_SERVER_IP "chat.bananymous.com"
#define DEFAULT_SERVER_PORT 6969

void usage() {
    printf("Usage: bananchat [-s SERVER_IP] [-p SERVER_PORT] <USERNAME>\n");
    printf("Client for communicating with Bananymous' chat server\n");
    exit(1);
}

void version() {
    printf("bananchat (Ethereal miniutils) 1.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int c;
    char *server_ip = DEFAULT_SERVER_IP;
    uint32_t server_port = DEFAULT_SERVER_PORT;

    while ((c = getopt(argc, argv, "s:p:hv")) != -1) {
        switch (c) {
            case 's':
                if (!optarg) {
                    printf("bananchat: option \'-s\' requires an argument\n");
                    return 1;
                }

                server_ip = optarg;
                break;
            case 'p':
                if (!optarg) {
                    printf("bananchat: option \'-p\' requires an argument\n");
                    return 1;
                }

                server_port = strtol(optarg, NULL, 10);
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

    if (argc - optind < 1) usage();

    printf("Establishing connection to %s:%d\n", server_ip, server_port);

    struct hostent *ent = gethostbyname(server_ip);
    if (!ent) {
        printf("dns-resolve: %s: not found by DNS\n", server_ip);
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
        .sin_port = htons(server_port),
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
    char buf[512];
    size_t bufidx = 0;
    while (1) {
        struct pollfd fds[2];
        fds[0].fd = sock;
        fds[0].events = POLLIN;
        
        fds[1].fd = STDIN_FILE_DESCRIPTOR;
        fds[1].events = POLLIN;

        int p = poll(fds, 2, 0);
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

        // Anything from stdin?
        if (p && fds[1].revents & POLLIN) {
            char c = getchar();
            putchar(c);
            fflush(stdout);

            if (c == '\n') {
                buf[bufidx++] = 0;

                if (send(sock, buf, strlen(buf), 0) < 0) {
                    perror("send");
                    return 1;
                }
                bufidx = 0;
            } else if (c) {
                buf[bufidx++] = c;
            }
        }
    }
}