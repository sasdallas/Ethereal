/**
 * @file hexahedron/include/kernel/drivers/net/icmp.h
 * @brief Internet Control Message Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_NET_ICMP_H
#define DRIVERS_NET_ICMP_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/ipv4.h>

/**** DEFINITIONS ****/
#define ICMP_ECHO_REPLY             0       // Echo reply
#define ICMP_DEST_UNREACHABLE       3       // Destination unreachable
#define ICMP_REDIRECT_MESSAGE       5       // Redirect message
#define ICMP_ECHO_REQUEST           8       // Echo request
#define ICMP_ROUTER_ADVERTISEMENT   9       // Router advertisement
#define ICMP_ROUTER_SOLICITATION    10      // Router solicitation
#define ICMP_TTL_EXCEEDED           11      // Time-to-live exceeded
#define ICMP_TRACEROUTE             30      // Traceroute

/**** TYPES ****/

/**
 * @brief ICMP packet
 */
typedef struct icmp_packet {
    uint8_t type;               // Packet type
    uint8_t code;               // ICMP code
    uint16_t checksum;          // Checksum
	uint32_t varies;            // Varies
    uint8_t data[];             // Data
} __attribute__((packed)) icmp_packet_t;

/**** FUNCTIONS ****/

/**
 * @brief Handle an IPv4 packet
 * @param nic_node The NIC that got the packet 
 * @param frame The frame that was received
 * @param size The size of the packet
 */
int icmp_handle(nic_t *nic_node, void *frame, size_t size);

#endif