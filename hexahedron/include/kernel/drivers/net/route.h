/**
 * @file hexahedron/include/kernel/drivers/net/route.h
 * @brief Network routing engine
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_NET_ROUTE_H
#define DRIVERS_NET_ROUTE_H

/**** INCLUDES ****/
#include <kernel/drivers/net/nic.h>
#include <ethereal/network.h>
#include <arpa/inet.h>
#include <stdint.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef struct _route_ipv4 {
    struct _route_ipv4 *next;
    nic_t *dev;
    in_addr_t dest;
    in_addr_t netmask;
    in_addr_t gw;
    unsigned short metric;
    unsigned short flags;
} route_ipv4_t;

typedef struct _route_ipv4_trie_entry {
    in_addr_t dest;
    in_addr_t mask;
    route_ipv4_t *route;
    unsigned char cidr_length;
    struct _route_ipv4_trie_entry *child[2];
} route_ipv4_trie_entry_t;


/**** FUNCTIONS ****/

/**
 * @brief Locate a route in the IPv4 routing table
 * @param ip The target destination IP (host byte order)
 * @returns Routing entry, or NULL if it could not be found
 */
route_ipv4_t *route_lookup(in_addr_t ip);

/**
 * @brief Add a route to the IPv4 routing table
 * @param route The route to add
 * @returns 0 on success
 */
int route_add(route_ipv4_t *route);

#endif
