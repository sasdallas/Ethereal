/**
 * @file libpolyhedron/include/netinet/in.h
 * @brief Internet address family
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header 


#ifndef _NETINET_IN_H
#define _NETINET_IN_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/socket.h>

/**** DEFINITIONS ****/

#define INADDR_ANY              0
#define INADDR_NONE             0xFFFFFFFF
#define INADDR_BROADCAST        0xFFFFFFFF
#define INADDR_LOOPBACK         0x7F000001

/**** TYPES ****/

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    unsigned int    sin_family;
    in_port_t       sin_port;
    struct in_addr  sin_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t     sin6_family;
    in_port_t       sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};



#endif

_End_C_Header