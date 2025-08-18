/**
 * @file userspace/miniutils/ping.c
 * @brief ping utility
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
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <time.h>


// PING ICMP header (identifier + sequence numbers)
typedef struct icmp_header {
    uint8_t type;
    uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence_number;
	uint8_t payload[];
} icmp_header_t;

/* Destination IP */
char *dest_ip = NULL;

/* seq/recv */
int seq = 0;
int rcvd = 0;

/**
 * @brief Calculate the ICMP checksum
 */
static uint16_t icmp_checksum(void *payload, size_t len) {
    uint16_t *p = (uint16_t*)payload;
    uint32_t checksum = 0;

    for (size_t i = 0; i < (len / 2); i++) checksum += ntohs(p[i]);
    if (checksum > 0xFFFF) {
        checksum = (checksum >> 16) + (checksum & 0xFFFF);
    }

    return ~(checksum & 0xFFFF) & 0xFFFF;
}

void sigint(int signum) {
    printf("--- %s ping statistics ---\n", dest_ip);
    printf("%d packets transmitted, %d received, %d%% packet loss\n", seq, rcvd, (100*(seq-rcvd)/seq));
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ping: Destination address required\n");
        return 1;
    }

    // Route the destination IP
    dest_ip = argv[1];

    // Register SIGINT handler
    signal(SIGINT, sigint);

    // Create a socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct hostent *ent = gethostbyname(dest_ip);
    if (!ent) {
        fprintf(stderr, "ping: %s: Error resolving\n", argv[1]);
        return 1;
    }

    // Create sockaddr_in
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    memcpy(&dest.sin_addr.s_addr, ent->h_addr_list[0], ent->h_length);

    // Setup the initial ping request
    icmp_header_t *ping_req = malloc(64);
    ping_req->type = 8;
    ping_req->code = 0;
    ping_req->identifier = 0;
    ping_req->sequence_number = 0;
    for (int i = 0; i < 64 - 8; i++) {
        ping_req->payload[i] = i;
    }    

    // PING!
    printf("PING %s (%s) 56 data bytes\n", dest_ip, ent->h_name);

    while (1) {
        seq++;

        // Setup the ping request
        ping_req->sequence_number = htons(seq);
        ping_req->checksum = 0;
        ping_req->checksum = htons(icmp_checksum((char*)ping_req, 64));

        // Send a ping and get the current time
        ssize_t s = sendto(sock, (void*)ping_req, 64, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in));
        if (s < 0) {
            perror("sendto");
            return 1;
        }


	    struct timeval tv;
	    gettimeofday(&tv,NULL);
	    unsigned long send_time = tv.tv_sec * 1000000L + tv.tv_usec;

        // Poll
        struct pollfd fds[1];
        fds[0].fd = sock;
        fds[0].events = POLLIN;

        int r = poll(fds, 1, 1000);
        if (!r) {
            printf("Poll timed out\n");
            continue;
        }

        // Fail?
        if (r < 0) {
            perror("poll");
            return 1;
        }

        // Receive
        char data[4096];
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);

        ssize_t bytes = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr*)&src, &src_len);
        
        // Receive time
	    gettimeofday(&tv,NULL);
	    unsigned long recv_time = tv.tv_sec * 1000000L + tv.tv_usec;

        if (bytes) {
            // Check to make sure its a ping packet
            icmp_header_t *hdr = (icmp_header_t*)data;

            if (hdr->type == 0) {
                // Yup, it's a ping response packet.
                int t = (recv_time - send_time);
                char *a = inet_ntoa(dest.sin_addr);
                printf("%d bytes from %s: icmp_seq=%d time=%d.%03dms\n", bytes, a, ntohs(hdr->sequence_number), t/1000, t%1000);
                rcvd++;
            }
        }

        sleep(1);
    }

    return 0;
}