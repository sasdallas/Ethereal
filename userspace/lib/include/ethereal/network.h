/**
 * @file userspace/lib/include/ethereal/network.h
 * @brief Ethereal Network Interface API
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _ETHEREAL_NETWORK_H
#define _ETHEREAL_NETWORK_H

/**** INCLUDES ****/
#include <arpa/inet.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define IO_NIC_GET_INFO     0x4001      // Get NIC information
#define IO_NIC_SET_INFO     0x4002      // Set NIC information
#define IO_NIC_ADD_ROUTE    0x4003      // Add route to global table (TODO this will not be here later)

// route flags
#define RT_FLAG_UP          0x1         // Route is UP

/**** TYPES ****/

typedef struct nic_info {
    char nic_name[256];                 // NIC name (hehe, "nickname")
    uint8_t nic_mac[6];                 // NIC MAC address
    size_t nic_mtu;                     // NIC MTU
    in_addr_t nic_ipv4_addr;            // NIC IPv4 address
    in_addr_t nic_ipv4_subnet;          // NIC IPv4 subnet
    in_addr_t nic_ipv4_gateway;         // NIC IPv4 gateway
} nic_info_t;

typedef struct net_route {
    char name[256];
    in_addr_t dest;
    in_addr_t gateway;
    in_addr_t mask;
    unsigned short flags;
    unsigned short metric;
} net_route_t;


#endif
