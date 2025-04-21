/**
 * @file libpolyhedron/include/netdb.h
 * @brief Network database
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

#ifndef _NETDB_H
#define _NETDB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/socket.h>

/**** DEFINITIONS ****/

#define AI_PASSIVE          0x01
#define AI_CANONNAME        0x02
#define AI_NUMERICHOST      0x04
#define AI_NUMERICSERV      0x08
#define AI_V4MAPPED         0x10
#define AI_ALL              0x20
#define AI_ADDRCONFIG       0x40

/**** TYPES ****/

typedef struct addrinfo {
    int             ai_flags;       // Input flags
    int             ai_family;      // Address family
    int             ai_socktype;    // Socket type
    int             ai_protocol;    // Protocol of socket
    socklen_t       ai_addrlen;     // Address length
    struct sockaddr *ai_addr;       // Address
    char            *ai_canonname;  // Canonical name
    struct addrinfo *ai_next;       // Pointer to next in list
};


#endif


_End_C_Header