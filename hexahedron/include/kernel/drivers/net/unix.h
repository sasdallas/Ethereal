/**
 * @file hexahedron/include/kernel/drivers/net/unix.h
 * @brief UNIX socket
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NET_UNIX_H
#define DRIVERS_NET_UNIX_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/socket.h>
#include <structs/list.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/net/socket.h>
#include <sys/un.h>
#include <structs/circbuf.h>

/**** DEFINITIONS ****/

#define UNIX_PACKET_BUFFER_SIZE     4096

/**** TYPES ****/

typedef struct unix_conn_request {
    sock_t *sock;                       // Incoming connection request socket
    sock_t *new_sock;                   // New socket flag (NULL until connection accepted)
} unix_conn_request_t;

typedef struct unix_datagram_data {
    size_t packet_size;                 // Packet size
    struct sockaddr_un un;              // UNIX address
} unix_datagram_data_t;

typedef struct unix_sock {
    sock_t *connected_socket;           // Connected socket
    struct thread *thr;                 // Thread to wakeup on new packet
    fs_node_t *bound;                   // Bound node
    char sun_path[108];                 // Path to socket
    char *map_path;                     // Map path

    circbuf_t *packet_buffer;           // Packet buffer
    list_t *dgram_data;                 // (datagram/seqpacket only) Packet sizes list

    spinlock_t incoming_connect_lock;   // Incoming socket connections lock
    list_t *incoming_connections;       // Incoming socket connections
} unix_sock_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the UNIX socket system
 */
void unix_init();

/**
 * @brief Create a UNIX socket
 * @param type The type
 * @param protocol The protocol
 */
sock_t *unix_socket(int type, int protocol);

/**
 * @brief Send a packet to a connected UNIX socket
 * @param sock The UNIX packet to send on
 * @param packet The packet to send
 * @param size Size of the packet
 * @returns 0 on success (for ordered will block until ACK if needed)
 */
int unix_sendPacket(sock_t *sock, void *packet, size_t size);

/**
 * @brief Read a packet from a UNIX socket
 * @param sock The UNIX socket to read from
 * @returns Received packet
 */
sock_recv_packet_t *unix_getPacket(sock_t *sock);

#endif