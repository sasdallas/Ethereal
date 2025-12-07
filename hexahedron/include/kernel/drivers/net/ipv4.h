/**
 * @file hexahedron/include/kernel/drivers/net/ipv4.h
 * @brief IPv4
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_NET_IPV4_H
#define DRIVERS_NET_IPV4_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/nic.h>
#include <netinet/in.h>

/**** DEFINITIONS ****/

#define IPV4_PACKET_TYPE        0x0800

/* Protocols */
#define IPV4_PROTOCOL_ICMP      1
#define IPV4_PROTOCOL_IGMP      2
#define IPV4_PROTOCOL_TCP       6
#define IPV4_PROTOCOL_UDP       17

/* Defaults */
#define IPV4_DEFAULT_TTL        64

/* Broadcast */
#define IPV4_BROADCAST_IP       0xffffffff  // 255.255.255.255


/**** TYPES ****/

/**
 * @brief IPv4 packet
 */
typedef struct ipv4_packet {
    uint8_t versionihl;             // Version and internet header length
    uint32_t dscp:6;                // Differentiated services code point
    uint32_t ecn:2;                 // Explicit congestion notification
    uint16_t length;                // Total length (header + data)
    uint16_t id;                    // Identification
    uint16_t offset;                // Fragment offset (also includes flags)
    uint8_t ttl;                    // Time to live
    uint8_t protocol;               // Protocol
    uint16_t checksum;              // Header checksum
    uint32_t src_addr;              // Source IPv4 address
    uint32_t dest_addr;             // Destination address
	uint8_t  payload[];
} __attribute__((packed)) __attribute__((aligned(2))) ipv4_packet_t;

/**
 * @brief IPv4 protocol handler
 * @param nic The NIC that received the packet
 * @param frame The payload that was received
 * @param size The size of the payload that was received
 * @returns 0 on success
 */
typedef int (*ipv4_handler_t)(fs_node_t *nic, void *frame, size_t size);

/**** FUNCTIONS ****/

/**
 * @brief Register an IPv4 protocol handler
 * @param protocol Protocol to handle
 * @param handler Handler
 * @returns 0 on success
 */
int ipv4_register(uint8_t protocol, ipv4_handler_t handler);

/**
 * @brief Unregister an IPv4 protocol handler
 * @param protocol Protocol to unregister
 * @returns 0 on success
 */
int ipv4_unregister(uint8_t protocol);

/**
 * @brief Calculate the IPv4 checksum
 * @param packet The packet to calculate the checksum of
 * @returns Checksum
 */
uint16_t ipv4_checksum(ipv4_packet_t *packet);

/**
 * @brief Send an IPv4 packet
 * @param nic The NIC to send the IPv4 packet to
 * @param dest Destination IPv4 address to send to
 * @param protocol Protocol to send, @c IPV4_PROTOCOL_xxx
 * @param frame Frame to send
 * @param size Size of the frame
 * @returns 0 on successful send
 */
int ipv4_send(fs_node_t *nic, in_addr_t dest, uint8_t protocol, void *frame, size_t size);

/**
 * @brief Send an IPv4 packet
 * @param nic The NIC to send the IPv4 packet to
 * @param packet The packet to send
 * @returns 0 on success
 */
int ipv4_sendPacket(fs_node_t *nic_node, ipv4_packet_t *packet);

/**
 * @brief Handle an IPv4 packet
 * @param frame The frame that was received (does not include the ethernet header)
 * @param nic_node The NIC that got the packet 
 * @param size The size of the packet
 */
int ipv4_handle(void *frame, fs_node_t *nic_node, size_t size);

#endif