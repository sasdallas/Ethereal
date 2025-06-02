/**
 * @file userspace/miniutils/test-tcp.c
 * @brief TCP sock testing
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: test-tcp [IP] [PORT]\n");
        return 1;
    }

    struct hostent *ent = gethostbyname(argv[1]);
    if (!ent) {
        herror("test-tcp");
        return 1;
    }

    // Make a socket
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
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
        .sin_family = AF_INET,
        .sin_port = htons(strtol(argv[2], NULL, 10))
    };

    memcpy(&dest.sin_addr.s_addr, ent->h_addr_list[0], ent->h_length);
    
    if (connect(sock, (const struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        return 1;
    }


    char *data = "Hello, world!";
    if (send(sock, (const void*)data, strlen(data), 0) < 0) {
        perror("send");
        return 1;
    }

    char data_back[4096];

    if (recv(sock, (void*)data_back, 4096, 0) < 0) {
        perror("recv");
        return 1;
    }  

    printf("Data: %s\n", data_back);

    close(sock);
    return 0;
}