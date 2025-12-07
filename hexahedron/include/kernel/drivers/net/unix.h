/**
 * @file hexahedron/include/kernel/drivers/net/unix.h
 * @brief UNIX socket implementation
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
#include <structs/circbuf.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/mutex.h>
#include <kernel/misc/spinlock.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/misc/util.h>
#include <structs/queue.h>

/**** TYPES ****/

typedef enum {
    UNIX_SOCK_STATE_INIT,
    UNIX_SOCK_STATE_BOUND,
    UNIX_SOCK_STATE_LISTEN,
    UNIX_SOCK_STATE_CONNECTING,
    UNIX_SOCK_STATE_CONNECTED,
    UNIX_SOCK_STATE_CLOSED
} unix_sock_state_t;

typedef enum {
    UNIX_CONN_WAITING,
    UNIX_CONN_CONNECTED,
    UNIX_CONN_DEAD,
} unix_conn_state_t;

typedef struct unix_sock_packet {
    struct unix_sock_packet *next;
    size_t data_size;
    char data[];
} unix_sock_packet_t;

typedef struct unix_packet_data {
    unix_sock_packet_t *rx_head;
    unix_sock_packet_t *rx_tail;
    spinlock_t rx_lock;
    sleep_queue_t *rx_wait_queue;
    sleep_queue_t *tx_wait_queue;
} unix_packet_data_t;

typedef struct unix_conn_req {
    unix_conn_state_t state;
    sock_t *socket;
    struct thread *thr;
} unix_conn_req_t;


typedef struct unix_sock {
    volatile uint8_t state;         // Socket state
    sock_t *sock;                   // Socket
    fs_node_t *node;                // Bound socket node
    char *un_path;                  // Node path
    struct unix_sock *peer;         // Peer connected UNIX socket
    bool is_listener;               // !!!: stupid hack

    // Type specific
    union {
        struct {
            unix_packet_data_t *d;
        } pkt;

        struct {
            circbuf_t *cb;          // Circular buffer
        } stream; // STREAM 

        struct {
            mutex_t *m;                 // Mutex
            queue_t *conn;              // Pending socket connections
            sleep_queue_t *accepters;   // Acceptor queue
        } server;
    }; 

    refcount_t ref;                 // Reference count
} unix_sock_t;

/**** DEFINITIONS ****/

#define UNIX_SOCKET_BUFFER_SIZE         8192

#endif