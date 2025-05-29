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
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>

/* hostent structure */
/* TODO: Are we supposed to be using this? It means that two calls of gethostbyname side-by-side will fail */
static uint32_t _hostent_addr = 0x0;
static char *_h_addr_list[2] = { (char*)&_hostent_addr, NULL };
static struct hostent _hostent = {
    .h_addr_list = _h_addr_list
};

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

    // TODO: DNS
    h_errno = HOST_NOT_FOUND;
    return NULL;
}