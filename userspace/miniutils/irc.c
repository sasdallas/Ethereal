/**
 * @file userspace/miniutils/irc.c
 * @brief IRC server
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

/* Socket file */
static FILE *fsock = NULL;

/* Current IRC channel */
static char *irc_channel = NULL;

/* Is command */
#define ISCMD(c) (strstr(cmd, "/" c) == cmd)

/**
 * @brief IRC client user
 */
void usage() {
    printf("Usage: irc [-n NICK] [IP] [PORT]\n");
    printf("Client for communicating with IRC servers\n");
    exit(1);
}

/**
 * @brief IRC client version
 */
void version() {
    printf("irc (Ethereal miniutils) 1.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Parse IRC command/input and send it
 * @param cmd The command to parse
 */
void irc_parseCommand(char *cmd) {
    // Is this a command?
    if (*cmd == '/') {
        if (ISCMD("join")) {
            // /join command to join a channel
            char *m = strstr(cmd, " ");
            if (!m) {
                printf("[Ethereal] Usage: /join channel\n");
                return;
            }
            m++;

            fprintf(fsock, "JOIN %s\r\n", m);
            fflush(fsock);
            if (irc_channel) free(irc_channel);
            irc_channel = strdup(m);
        } else if (ISCMD("help")) {
            printf("[Ethereal] Help not available\n");
            printf("[Ethereal] (leave me alone)\n");
        } else {
            printf("[Ethereal] Unrecognized command: \"%s\"\n", cmd);
        }
    } else {
        // No, send it to the current channel
        if (!irc_channel) {
            // We aren't in a channel
            printf("[Ethereal] You aren't in an IRC channel - use /join to join a channel!\n");
            return;
        }

        fprintf(fsock, "PRIVMSG %s :%s\r\n", irc_channel, cmd);
        fflush(fsock);
    }
}

/**
 * @brief Parse IRC response and display it
 * @param resp The response to parse
 */
void irc_parseResponse(char *response) {
    printf("%s", response);
    fflush(stdout);

    if (strstr(response, "PING")) {
        // Oh shoot, they are pinging us!
        char *r = strstr(response, ":");
        if (!r) {
            printf("[Ethereal] WARNING: Invalid PING request received\n");
            return;
        }

        fprintf(fsock, "PONG %s\r\n", r);
        fflush(fsock);
    }
}

int main(int argc, char *argv[]) {
    char *nick = "EtherealUser";
    
    // Parse any options given to us
    char c;
    while ((c = getopt(argc, argv, "n:hv")) != -1) {
        switch (c) {
            case 'n':
                // Use custom nick
                if (!optarg) {
                    printf("irc: option \'-n\' requires an argument\n");
                    return 1;
                }

                nick = optarg;
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

    if (argc - optind < 2) usage();

    // Get IP and port
    char *server_ip = argv[optind];
    uint32_t server_port = strtol(argv[optind+1], NULL, 10);

    // Establish connection
    printf("Establishing connection to %s:%d\n", server_ip, server_port);
    struct hostent *ent = gethostbyname(server_ip);
    if (!ent) {
        printf("dns-resolve: %s: not found by DNS\n", server_ip);
        return 1;
    }

    // Create socket
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


    // Connect to server
    if (connect(sock, (const struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to IRC server successfully\n");

    // Open socket as file and inform server of our nickname
    fsock = fdopen(sock, "rw");
    fprintf(fsock, "NICK %s\r\nUSER %s * 0 :%s\r\n", nick, nick, nick);
    fflush(fsock);

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
            // irc_parseResponse(data);
        }

        // Anything from stdin?
        if (p && fds[1].revents & POLLIN) {
            char c = getchar();
            putchar(c);
            fflush(stdout);

            if (c == '\n') {
                // We're done, flush buffer and goto command handler
                buf[bufidx++] = 0;
                bufidx = 0;
                irc_parseCommand(buf);
            } else if (c) {
                buf[bufidx++] = c;
            }
        }
    }
}