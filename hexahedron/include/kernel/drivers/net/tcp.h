/**
 * @file hexahedron/include/kernel/drivers/net/tcp.h
 * @brief Transmission Control Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NET_TCP_H
#define DRIVERS_NET_TCP_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/ipv4.h>
#include <kernel/task/sleep.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

#define TCP_FLAG_FIN        (1 << 0)    // Final packet from sender
#define TCP_FLAG_SYN        (1 << 1)    // Synchronize
#define TCP_FLAG_RST        (1 << 2)    // Reset
#define TCP_FLAG_PSH        (1 << 3)    // Asks to push buffered data to receiving application
#define TCP_FLAG_ACK        (1 << 4)    // ACKNOWLEDGE!
#define TCP_FLAG_URG        (1 << 5)    // This data is probably pretty sensitive
#define TCP_FLAG_ECE        (1 << 6)    // Either ECN capable or a packet with Congestion Experienced was received
#define TCP_FLAG_CWR        (1 << 7)    // Congestion window reduced

#define TCP_STATE_DEFAULT       0
#define TCP_STATE_LISTEN        1
#define TCP_STATE_SYN_SENT      2
#define TCP_STATE_SYN_RECV      3
#define TCP_STATE_ESTABLISHED   4
#define TCP_STATE_FIN_WAIT1     5
#define TCP_STATE_FIN_WAIT2     6
#define TCP_STATE_CLOSE_WAIT    7
#define TCP_STATE_CLOSING       8
#define TCP_STATE_LAST_ACK      9
#define TCP_STATE_CLOSED        10

#define TCP_DEFAULT_WINSZ       65535

#define TCP_HEADER_LENGTH_MASK  0xF000
#define TCP_HEADER_LENGTH_SHIFT 12



/**** TYPES ****/

typedef struct tcp_packet {
    uint16_t src_port;                  // Source port
    uint16_t dest_port;                 // Destination port
    uint32_t seq;                       // Sequence number
    uint32_t ack;                       // Acknowledge number
    uint16_t flags;                     // Flags
    uint16_t winsz;                     // Window size
    uint16_t checksum;                  // Checksum
    uint16_t urgent;                    // Urgent pointer
    uint8_t payload[];                  // Payload data
} __attribute__((packed)) tcp_packet_t;

typedef struct tcp_checksum_header {
    uint32_t src;                       // Source IPv4
    uint32_t dest;                      // Destination IPv4
    uint8_t reserved;                   // Reserved
    uint8_t protocol;                   // Protocol
    uint16_t length;                    // Length
    uint8_t payload[];                  // Payload
} __attribute__((packed)) tcp_checksum_header_t;

typedef struct tcp_sock {
    uint16_t port;                      // Port bound to this socket
    uint8_t state;                      // TCP state
    uint32_t seq;                       // Sequence number
    uint32_t ack;                       // Acknowledge number

    // Pending connections
    spinlock_t pending_lock;            // Pending connections lock
    list_t *pending_connections;        // Pending connections
    sleep_queue_t *accepting_queue;     // Accepting thread queue
} tcp_sock_t;

/**** FUNCTIONS ****/

/**
 * @brief Handle a TCP packet
 */
int tcp_handle(fs_node_t *nic, void *frame, size_t size);

#endif