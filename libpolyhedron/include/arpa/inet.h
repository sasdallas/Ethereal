/**
 * @file libpolyhedron/include/arpa/inet.h
 * @brief ARPA inet
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

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

/**** INCLUDES ****/
#include <stdint.h>
#include <netinet/in.h>

/**** DEFINITIONS ****/

#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

/**** FUNCTIONS ****/

uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);

uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

in_addr_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);

#endif

_End_C_Header