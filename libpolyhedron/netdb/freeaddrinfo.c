/**
 * @file libpolyhedron/netdb/freeaddrinfo.c
 * @brief freeaddrinfo
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
#include <stdlib.h>

void freeaddrinfo(struct addrinfo *ai) {
    struct addrinfo *p = ai;
    while (p) {
        struct addrinfo *next = p->ai_next;
        if (p->ai_addr) free(p->ai_addr);
        free(p);
        p = next;    
    }
}