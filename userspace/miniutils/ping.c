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

// PING ICMP header (identifier + sequence numbers)
typedef struct icmp_header {
    uint8_t type, code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence_number;
	uint8_t payload[];
} icmp_header_t;

/**
 * @brief Get the current time
 */
static unsigned long get_current_time(void) {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ping: Destination address required\n");
        return 1;
    }

    // Route the destination IP
    char *dest_ip = argv[1];

    struct hostent *ent = gethostbyname(dest_ip);
    if (!ent) {
        herror("ping");
        return 1;
    }

    // Create a socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) {
        perror("socket");
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

    
    int seq = 0;
    int rcvd = 0;
    for (; seq < 5; seq++) {
        // Setup the ping request
        ping_req->sequence_number = htons(seq+1);
        ping_req->checksum = 0;
        ping_req->checksum = htons(icmp_checksum((char*)ping_req, 64));

        // Send a ping and get the current time
        unsigned long send_time = get_current_time();
        ssize_t s = sendto(sock, (void*)ping_req, 64, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in));
        if (s < 0) {
            perror("sendto");
            return 1;
        }

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
        struct iovec iov = {
            .iov_base = data,
            .iov_len = 4096
        };

        struct msghdr msg = {
            .msg_control = NULL,
            .msg_controllen = 0,
            .msg_name = &dest,
            .msg_namelen = sizeof(struct sockaddr_in),
            .msg_iov = &iov,
            .msg_iovlen = 1
        };

        ssize_t bytes = recvmsg(sock, &msg, 0);
        unsigned long recv_time = get_current_time();

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

    printf("--- %s ping statistics ---\n", dest_ip);
    printf("%d packets transmitted, %d received, %d%% packet loss\n", seq, rcvd, (100*(seq-rcvd)/seq));
    close(sock);
    return 0;
}