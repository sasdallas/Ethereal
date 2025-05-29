/**
 * @file hexahedron/include/kernel/drivers/nicdev.h
 * @brief NIC device ioctls
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NICDEV_H
#define DRIVERS_NICDEV_H

/**** INCLUDES ****/
#include <stdint.h>
#include <netinet/in.h>

/**** TYPES ****/

typedef struct nic_info {
    char nic_name[256];                 // NIC name (hehe, "nickname")
    uint8_t nic_mac[6];                 // NIC MAC address
    size_t nic_mtu;                     // NIC MTU
    in_addr_t nic_ipv4_addr;            // NIC IPv4 address
    in_addr_t nic_ipv4_subnet;          // NIC IPv4 subnet
    in_addr_t nic_ipv4_gateway;         // NIC IPv4 gateway
} nic_info_t;

/**** DEFINITIONS ****/
#define IO_NIC_GET_INFO     0x4001      // Get NIC information
#define IO_NIC_SET_INFO     0x4002      // Set NIC information

#endif  