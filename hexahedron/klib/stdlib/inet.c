/**
 * @file hexahedron/klib/stdlib/inet.c
 * @brief inet functions
 * @note this isn't part of stdlib
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char __tmp_buffer[17] = { 0 };

uint32_t ntohl(uint32_t netlong) { return ((netlong & 0xFF000000) >> 24) | ((netlong & 0xFF0000) >> 8) | ((netlong & 0xFF00) << 8) | ((netlong & 0xFF) << 24); }
uint16_t ntohs(uint16_t netshort) { return ((netshort & 0xFF) << 8) | ((netshort & 0xFF00) >> 8); }

uint32_t htonl(uint32_t hostlong) { return ((hostlong & 0xFF) << 24) | (((hostlong & 0xFF00) >> 8) << 16) | (((hostlong & 0xFF0000) >> 16) << 8) | (((hostlong & 0xFF000000) >> 24)); }
uint16_t htons(uint16_t hostshort) { return ((hostshort & 0xFF) << 8) | (((hostshort & 0xFF00) >> 8)); }

static char __inet_ntoa_buf[256] = { 0 };
char *inet_ntoa(struct in_addr in) {
    unsigned char *bytes;
    in.s_addr = ntohl(in.s_addr); // Convert to host byte order
    bytes = (unsigned char *)&in.s_addr;
    snprintf(__inet_ntoa_buf, sizeof(__inet_ntoa_buf), "%u.%u.%u.%u",
             bytes[0], bytes[1], bytes[2], bytes[3]);
    return __inet_ntoa_buf;
}

in_addr_t inet_addr(const char *cp) {
    strncpy(__tmp_buffer, cp, 17);
    char *p = (char*)__tmp_buffer;

    // IPv4 addresses are formatted X.X.X.X (up to 3 numbers, 3 dots)
    uint32_t addr = 0;
    int dots = 0;
    while (*p) {
        char *lp = p;
        char *d = strchrnul(p, '.');

        if ((int)(d-lp) == 0 || (int)(d-lp) > 3) return INADDR_NONE;
        if (*d == '.') dots++;
        if (dots > 3) return INADDR_NONE;

        // Shift addr over
        addr <<= 8;

        // Save and restore d
        char d_saved = *d;
        *d = 0;
        addr |= strtol(p, NULL, 10);
        *d = d_saved;

        if (!(*d)) break;
        p = d+1;
    }

    if (dots != 3) {
        return INADDR_NONE;
    }

    return htonl(addr);
}