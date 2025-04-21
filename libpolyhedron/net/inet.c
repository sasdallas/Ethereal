/**
 * @file libpolyhedron/net/inet.c
 * @brief Byte-order stuff
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint32_t htonl(uint32_t hostlong) {
	return (
        (((hostlong) & 0xFF) << 24) | 
        (((hostlong) & 0xFF00) << 8) | 
        (((hostlong) & 0xFF0000) >> 8) | 
        (((hostlong) & 0xFF000000) >> 24)
    );
}

uint16_t htons(uint16_t hostshort) {
    return (
        (((hostshort) & 0xFF) << 8) |
        (((hostshort) & 0xFF00) >> 8)
    );
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

in_addr_t inet_addr(const char *cp) {
    if (!cp) return 0;

    // We cannot modify cp in any way, so copy it to a local buffer
    char ip[17];
    strncpy(ip, cp, 17);

    int values[4] = { 0 }; // Buffer to hold output integers

    // Loop
    char *p = ip;
    for (int i = 0; i < 4; i++) {
        // Find the occurence of a '.'
        char *next = strchr(p, '.');
        if (next == NULL) {
            // If we're at the end of our list, we don't care
            if (i == 3) {
                values[i] = atoi(p);
                break;
            }    

            return 0;
        }

        // Null the character out
        *next = 0;
        next++;

        // Now convert to integer
        values[i] = atoi(p);

        // Next
        p = next;
    }

    return htonl(((values[0] & 0xFF) << 24) | ((values[1] & 0xFF) << 16) | ((values[2] & 0xFF) << 8) | ((values[3] & 0xFF) << 0));
}

char *inet_ntoa(struct in_addr in) {
    static char buffer[17];

    // Convert byte order
    uint32_t in_fixed = ntohl(in.s_addr);
    
    // Copy
    // TODO: I don't like this
    snprintf(buffer, 17, "%d.%d.%d.%d",
            (in_fixed >> 24) & 0xFF,
            (in_fixed >> 16) & 0xFF,
            (in_fixed >> 8) & 0xFF,
            (in_fixed >> 0) & 0xFF);
    
    return buffer; // TODO: I really don't like this
}