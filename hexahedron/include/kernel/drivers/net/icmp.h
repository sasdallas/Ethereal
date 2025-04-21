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
 * @brief Initialize and register ICMP
 */
void icmp_init();

/**
 * @brief Send an ICMP packet
 * @param nic_node The NIC to send the ICMP packet to
 * @param dest_addr Target IP address
 * @param type Type of ICMP packet to send
 * @param code ICMP code to send
 * @param varies What should be put in the 4-byte varies field
 * @param data The data to send
 * @param size Size of packet to send
 * @returns 0 on success
 */
int icmp_send(fs_node_t *nic_node, in_addr_t dest, uint8_t type, uint8_t code, uint32_t varies, void *data, size_t size);

/**
 * @brief Handle an IPv4 packet
 * @param nic_node The NIC that got the packet 
 * @param frame The frame that was received
 * @param size The size of the packet
 */
int icmp_handle(fs_node_t *nic_node, void *frame, size_t size);

/**
 * @brief Ping!
 * @param nic_node The NIC to send the ping request on
 * @param addr The address to send the ping request to
 * @returns 0 on success
 */
int icmp_ping(fs_node_t *nic_node, in_addr_t addr);

#endif