/**
 * @file libpolyhedron/socket/gethostbyname.c
 * @brief gethostbyname
 * 
 * This function is technically obsolete but many programs do still need it.
 * 
 * This will do a lookup request for a given host name and return a struct hostent
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
#include <sys/dns.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <errno.h>

/* hostent structure */
/* TODO: Are we supposed to be using this? It means that two calls of gethostbyname side-by-side will fail */
static uint32_t _hostent_addr = 0x0;
static char *_h_addr_list[2] = { (char*)&_hostent_addr, NULL };
static struct hostent _hostent = {
    .h_addr_list = _h_addr_list
};

/* DNS push */
#define DNS_PUSH(val) { header->data[idx++] = val; } 


struct hostent *gethostbyname(const char *name) {
    // First, is name an IP address?
    // IPs must have 3 dots and at most 3 numbers between the dots
    int dots = 0;
    int digits = 0;
    int is_ip = 1; // Guilty until proven innocent
    char *p = (char*)name;
    while (*p) {
        if (isdigit(*p)) {
            digits++;
            if (digits > 3) {
                is_ip = 0;
                break;
            }
        } else if (*p == '.') {
            dots++;
            digits = 0;
            if (dots > 3) {
                is_ip = 0;
                break;
            }
        } else {
            is_ip = 0;
            break;
        }

        p++;
    }

    // Was it an IP?
    if (is_ip) {
        // Yes, construct hostent
        _hostent_addr = inet_addr(name);
        _hostent.h_name = (char*)name;
        _hostent.h_aliases = NULL;
        _hostent.h_addrtype = AF_INET;
        _hostent.h_length = sizeof(uint32_t);
        return &_hostent;
    }

    // No, but maybe it's localhost?
    if (!strcmp(name, "localhost")) {
        _hostent_addr = inet_addr("127.0.0.1");
        _hostent.h_name = (char*)name;
        _hostent.h_aliases = NULL;
        _hostent.h_addrtype = AF_INET;
        _hostent.h_length = sizeof(uint32_t);
        return &_hostent;
    }

    // First, get a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return NULL;
    }

    // Read the DNS configuration from /etc/resolv.conf
    in_addr_t dns_serv = inet_addr("8.8.8.8"); // TODO

    // Make a DNS request
    char dns_request[128];
    dns_header_t *header = (dns_header_t*)dns_request;
    
    // Setup header data (we are requesting one question, doing a QUERY and want recursion)
    // RNG a new transaction ID
    uint16_t xid = rand() & 0xFFFF;

    header->xid = htons(xid);
    header->additional = 0;
    header->flags = htons(DNS_FLAG_RD);
    header->questions = htons(1);
    header->answers = 0;
    header->authorities = 0;

    // Now we need to start building our DNS request
    size_t idx = 0;

    // First, push the host name that we want
    p = (char*)name;
    char *s = strchrnul(p, '.');
    while (1) {
        // Push the length first
        int is_end = !(*s);
        *s = 0;
        DNS_PUSH(strlen(p));

        // Now push the rest of p
        while (*p) {
            DNS_PUSH(*p);
            p++;
        }

        // Next
        if (is_end) break;
        else *s = '.'; // Fix character
        p = s+1;
        s = strchrnul(p, '.');
    }

    // Next, tell DNS we are providing a HOST address
    DNS_PUSH(0x00);
    DNS_PUSH(0x00);
    DNS_PUSH(0x01);

    // Now, say that this is an IN class
    DNS_PUSH(0x00);
    DNS_PUSH(0x01);

    // All done, the header is ready to be sent
    struct sockaddr_in dest = {
        .sin_addr.s_addr = dns_serv,
        .sin_port = htons(53),
        .sin_family = AF_INET
    };

    // Send!
    if (sendto(sock, header, idx + 1 + sizeof(dns_header_t), 0, (const struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "gethostbyname: DNS resolution error, could not send DNS request\n");
        fprintf(stderr, "gethostbyname: error: %s\n", strerror(errno));
        h_errno = NO_RECOVERY;
        return NULL;
    }

    // Poll socket until response
    struct pollfd fds[1];
    fds[0].fd = sock;
    fds[0].events = POLLIN;

    if (poll(fds, 1, 3000) <= 0) {
        h_errno = NO_RECOVERY;
        return NULL;
    }

    // Read!
    char resp_data[256];
    if (recv(sock, resp_data, 256, 0) < (ssize_t)sizeof(dns_header_t)) {
        fprintf(stderr, "gethostbyname: DNS resolution error, could not receive DNS request\n");
        fprintf(stderr, "gethostbyname: error: %s\n", strerror(errno));
        h_errno = NO_RECOVERY;
        return NULL;
    } 

    dns_header_t *resp = (dns_header_t*)resp_data;

    // Check to see if we got any answers
    if (!ntohs(resp->answers)) {
        fprintf(stderr, "gethostbyname: No hosts for %s\n", name);
        h_errno = NO_DATA;
        return NULL;
    }

    for (unsigned int i = 0; i < ntohs(resp->answers); i++) {
        // We got an answer, let's get it
        idx += 4; // Skip over name and type
        uint16_t class = resp->data[idx] * 255 + resp->data[idx+1];
        idx += 6; // Skip over TTL
        uint16_t len = resp->data[idx] * 255 + resp->data[idx+1];
        idx += 2;

        // Is this the len and class we want?
        if (len == 4 && class == 1) {
            // Yup! Get it
            _hostent_addr = *(uint32_t*)&resp->data[idx];
            _hostent.h_name = (char*)name;
            _hostent.h_aliases = NULL;
            _hostent.h_addrtype = AF_INET;
            _hostent.h_length = sizeof(uint32_t);
            return &_hostent;
        }

        idx += len;
    }

    h_errno = HOST_NOT_FOUND;
    return NULL;
}