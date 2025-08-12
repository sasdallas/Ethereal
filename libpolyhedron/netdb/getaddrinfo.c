/**
 * @file libpolyhedron/netdb/getaddrinfo.c
 * @brief getaddrinfo
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <netdb.h>
#include <sys/libc_debug.h>
#include <sys/dns.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>


int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo** res) {
    struct hostent *ent = gethostbyname(nodename);
    if (!ent) return EAI_FAIL; // TODO

    struct addrinfo *r = malloc(sizeof(struct addrinfo));
    if (!r) return EAI_MEMORY;

    r->ai_flags = 0;
    r->ai_family = AF_INET;
    r->ai_socktype = hints ? hints->ai_socktype : 0;
    r->ai_protocol = hints ? hints->ai_protocol : 0;
    r->ai_canonname = strdup(ent->h_name);
    if (!r->ai_canonname) {
        free(r);
        return EAI_MEMORY;
    }
    r->ai_next = NULL;

    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    if (!addr) {
        free(r->ai_canonname);
        free(r);
        return EAI_MEMORY;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = servname ? htons(atoi(servname)) : 0;
    memcpy(&addr->sin_addr, ent->h_addr, ent->h_length);

    r->ai_addr = (struct sockaddr *)addr;
    r->ai_addrlen = sizeof(struct sockaddr_in);

    *res = r;
    return 0;
}