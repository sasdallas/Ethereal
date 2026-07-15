/**
 * @file hexahedron/include/kernel/drivers/net/udp.h
 * @brief User Datagram Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NET_UDP_H
#define DRIVERS_NET_UDP_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/ipv4.h>
#include <structs/ringbuffer.h>
#include <kernel/refcount.h>

/**** TYPES ****/

typedef struct udp_packet {
    uint16_t src_port;              // Source port
    uint16_t dest_port;             // Destination port
    uint16_t length;                // Length
    uint16_t checksum;              // Checksum
    uint8_t data[];                 // UDP data
} __attribute__((packed)) udp_packet_t;

typedef struct udp_packet_info {
    in_addr_t src_addr;
    in_port_t src_port;
    uint16_t length;
} udp_packet_info_t;

typedef struct udp_sock {
    mutex_t lock;
    poll_event_t event;
    refcount_t refs; // !!! overkill

    struct sockaddr_in conn;
    uint32_t addr;
    uint16_t port;

    struct {
        // actual packet data
        ringbuffer_t *data;

        // circular buffer of info but cooler
        udp_packet_info_t *info;
        unsigned int info_head;
        unsigned int info_tail;
        unsigned int info_size;
    } pkts;
} udp_socket_t;


/**** FUNCTIONS ****/

/**
 * @brief Handle a UDP packet
 */
int udp_handle(nic_t *nic, void *frame, size_t size);

#endif