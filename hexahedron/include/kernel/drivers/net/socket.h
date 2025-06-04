/**
 * @file hexahedron/include/kernel/drivers/net/socket.h
 * @brief Network socket handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_NET_SOCKET_H
#define DRIVERS_NET_SOCKET_H

/**** INCLUDES ****/
#include <kernel/drivers/net/ipv4.h>
#include <kernel/drivers/net/nic.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <kernel/task/process.h>

/**** DEFINITIONS ****/

/* Socket flags */
/* These are just SO_ options that take booleans */
#define SOCKET_FLAG_DEBUG               0x01
#define SOCKET_FLAG_BROADCAST           0x02
#define SOCKET_FLAG_REUSEADDR           0x04
#define SOCKET_FLAG_KEEPALIVE           0x08
#define SOCKET_FLAG_OOBINLINE           0x10
#define SOCKET_FLAG_DONTROUTE           0x20
#define SOCKET_FLAG_NONBLOCKING         0x40

/**** TYPES ****/

struct sock;

// Socket methods
typedef ssize_t (*sock_sendmsg_t)(struct sock *sock, struct msghdr *message, int flags);
typedef ssize_t (*sock_recvmsg_t)(struct sock *sock, struct msghdr *message, int flags);
typedef int (*sock_bind_t)(struct sock *sock, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*sock_connect_t)(struct sock *sock, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*sock_accept_t)(struct sock *sock, struct sockaddr *addr, socklen_t *addrlen);
typedef int (*sock_listen_t)(struct sock *sock, int backlog);
typedef int (*sock_close_t)(struct sock *sock);


/**
 * @brief Socket object
 */
typedef struct sock {
    fs_node_t *node;                    // Node object
    int flags;                          // Flags
    int id;                             // Identifier of the socket (auto-assigned and can be resolved with socket_fromID)

    int domain;                         // Domain of the socket
    int type;                           // Type of the socket
    int protocol;                       // Protocol of the socket

    // METHODS
    sock_sendmsg_t sendmsg;             // sendmsg
    sock_recvmsg_t recvmsg;             // recvmsg
    sock_bind_t bind;                   // bind
    sock_connect_t connect;             // connect
    sock_listen_t listen;               // listen
    sock_accept_t accept;               // accept
    sock_close_t close;                 // close

    // RECEIVE
    spinlock_t *recv_lock;              // Receive lock
    sleep_queue_t *recv_wait_queue;     // Receive sleep queue
    list_t *recv_queue;                 // Received packet queue

    // OTHER
    struct sockaddr *connected_addr;    // Connected socket address
    socklen_t connected_addr_len;       // Connected address length
    nic_t *bound_nic;                   // Bound NIC
    void *driver;                       // Socket driver specific object
} sock_t;

/**
 * @brief Socket received packet
 */
typedef struct sock_recv_packet {
    size_t size;                        // Size of packet
    uint8_t data[];                     // Variable size
} sock_recv_packet_t;

/**
 * @brief Socket creation function
 * @param type The type of socket
 * @param protocol The protocol to use while creating the socket
 * @returns Socket object on success, NULL on error
 * 
 * You should setup the method table and your driver specific field.
 * If you leave @c sock->recv_queue or @c sock->recv_lock as NULL, they will be allocated for you.
 */
typedef sock_t* (*socket_create_t)(int type, int protocol);

/**** FUNCTIONS ****/

/**
 * @brief Initialize the socket system
 */
void socket_init();

/**
 * @brief Register a new handler for a socket type
 * @param domain The domain of the handler to register
 * @param socket_create Socket creation function
 * @returns 0 on success
 */
int socket_register(int domain, socket_create_t socket_create);

/**
 * @brief Create a new socket for a given process
 * @param proc The process to create the socket for
 * @param domain The domain to use while creating
 * @param type The type of the socket
 * @param protocol The protocol to use when creating the socket
 * @returns -ERRNO on error and fd on success
 */
int socket_create(process_t *proc, int domain, int type, int protocol);

/**
 * @brief Wait for received content to be available in a socket
 * @param sock The socket to wait on received content for
 * @returns 0 on success, 1 on interrupted
 */
int socket_waitForContent(sock_t *sock);

/**
 * @brief Write a packet to a socket and alert those who are waiting on it
 * @param sock The socket to write data to
 * @param data The data to write to the socket
 * @param size How much data to write to the socket
 * 
 * The socket can pull this information with @c socket_get 
 */
int socket_received(sock_t *sock, void *data, size_t size);

/**
 * @brief Wait for and get received packets from a socket
 * @param sock The socket to wait on packets from
 * @returns A packet on success, NULL on failure (if NULL assume -EINTR)
 * 
 * @note You are expected to free this data
 */
sock_recv_packet_t *socket_get(sock_t *sock);

/**
 * @brief Socket sendmsg method
 * @param socket The socket file descriptor
 * @param message Contains the message to be sent
 * @param flags Specifies the type of message transmission
 */
ssize_t socket_sendmsg(int socket, struct msghdr *message, int flags);

/**
 * @brief Socket recvmsg method
 * @param socket The socket file descriptor
 * @param message Contains the message buffer to store received one in
 * @param flags Specifies the type of message receive
 */
ssize_t socket_recvmsg(int socket, struct msghdr *message, int flags);

/**
 * @brief Socket setsockopt method
 * @param socket The socket file descriptor
 * @param level The protocol level for the option
 * @param option_name The name of the option
 * @param option_value The value of the option
 * @param option_len The length of the option
 */
int socket_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);

/**
 * @brief Socket bind method
 * @param socket The socket file descriptor
 * @param addr The address to bind to
 * @param addrlen The address length
 */
int socket_bind(int socket, const struct sockaddr *addr, socklen_t addrlen);

/**
 * @brief Socket connect method
 * @param socket The socket file descriptor
 * @param addr The address connect to
 * @param addrlen The address length
 */
int socket_connect(int socket, const struct sockaddr *addr, socklen_t addrlen);

/**
 * @brief Socket listen method
 * @param socket The socket file descriptor
 * @param backlog Backlog
 */
int socket_listen(int socket, int backlog);

/**
 * @brief Socket accept method
 * @param socket The socket file descriptor
 * @param addr The address to store the connection in
 * @param addrlen The address length
 */
int socket_accept(int socket, struct sockaddr *addr, socklen_t *addrlen);

/**
 * @brief Get a socket by its ID
 * @param id The ID to look for
 * @returns The socket or NULL
 * @warning This can be kinda slow since it searches the full socket list
 */
sock_t *socket_fromID(int id);

#endif