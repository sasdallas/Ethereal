/**
 * @file hexahedron/drivers/net/socket.c
 * @brief Socket handler
 * 
 * This includes support for @c AF_RAW sockets
 * 
 * @todo Add timeouts
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/net/socket.h>
#include <kernel/task/syscall.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <structs/hashmap.h>
#include <errno.h>

/* Handler map */
hashmap_t *socket_map = NULL;

/* Socket list */
list_t *socket_list = NULL;

/* Last socket ID */
int last_socket_id = 0;

/* Validate socket option */
#define SOCKET_VALIDATE_OPT(optvalue, optlen) SYSCALL_VALIDATE_PTR_SIZE(optvalue, optlen)

/* Set socket option in flags */
#define SOCKET_CHANGE_FLAG(flag, value) { if (value) { sock->flags |= flag; } else { sock->flags &= ~(flag); } };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NET:SOCKET", __VA_ARGS__)

/**
 * @brief Raw socket sendmsg method
 */
static ssize_t socket_raw_sendmsg(sock_t *sock, struct msghdr *message, int flags) {
    // Is this socket bound?
    if (!sock->bound_nic) return -EINVAL;

    // For each iovec...
    size_t total_sent = 0;
    LOG(DEBUG, "RAW: Sending message\n");
    for (int i = 0; i < message->msg_iovlen; i++) {
        // Try to send some data to the NIC
        total_sent += fs_write(sock->bound_nic->parent_node, 0, message->msg_iov[i].iov_len, message->msg_iov[i].iov_base);
    }

    return total_sent;
}

/**
 * @brief Raw socket recvmsg
 */
static ssize_t socket_raw_recvmsg(sock_t *sock, struct msghdr *message, int flags) {
    // Is this socket bound?
    if (!sock->bound_nic) return -EINVAL;

    // For each iovec...
    ssize_t total_received = 0;
    LOG(DEBUG, "RAW: Receiving message\n");
    for (int i = 0; i < message->msg_iovlen; i++) {
        // Try to get some data
        sock_recv_packet_t *pkt = socket_get(sock);
        if (!pkt) return -EINTR;

        // TODO: Handle size mismatch better?
        if (pkt->size > message->msg_iov[i].iov_len) {
            // TODO: Set MSG_TRUNC
            LOG(WARN, "Truncating packet from %d -> %d\n", pkt->size, message->msg_iov[i].iov_len);
            memcpy(message->msg_iov[i].iov_base, pkt->data, message->msg_iov[i].iov_len);
            total_received += message->msg_iov[i].iov_len;
            continue;
        }

        // Copy it and free the packet
        memcpy(message->msg_iov[i].iov_base, pkt->data, pkt->size);
        total_received += pkt->size;
        kfree(pkt);
    }

    return total_received;
}


/**
 * @brief Raw socket create method
 */
static sock_t *socket_raw_create(int type, int protocol) {
    sock_t *sock = kzalloc(sizeof(sock_t));

    sock->sendmsg = socket_raw_sendmsg;
    sock->recvmsg = socket_raw_recvmsg;

    return sock;
}

/**
 * @brief Initialize the socket system
 */
void socket_init() {
    socket_map = hashmap_create_int("socket map", 4);
    socket_list = list_create("socket list");
    socket_register(AF_RAW, socket_raw_create);
    LOG(INFO, "Sockets initialized\n");
}

/**
 * @brief Register a new handler for a socket type
 * @param domain The domain of the handler to register
 * @param socket_create Socket creation function
 * @returns 0 on success
 */
int socket_register(int domain, socket_create_t socket_create) {
    hashmap_set(socket_map, (void*)(uintptr_t)domain, (void*)socket_create);
    return 0;
}

/**
 * @brief Validate message method
 * @param msg The message to validate
 */
static int socket_validateMsg(struct msghdr *message) {
    SYSCALL_VALIDATE_PTR_SIZE(message, sizeof(struct msghdr));
    
    if (message->msg_control) SYSCALL_VALIDATE_PTR_SIZE(message->msg_control, message->msg_controllen);
    if (message->msg_name) SYSCALL_VALIDATE_PTR_SIZE(message->msg_name, message->msg_namelen);

    LOG(DEBUG, "CTRL: %p %d\n", message->msg_control, message->msg_controllen);
    LOG(DEBUG, "NAME: %p %d\n", message->msg_name, message->msg_namelen);

    if (message->msg_iovlen) {
        SYSCALL_VALIDATE_PTR_SIZE(message->msg_iov, message->msg_iovlen * sizeof(struct iovec));

        // For each iov
        for (int i = 0; i < message->msg_iovlen; i++) {
            SYSCALL_VALIDATE_PTR_SIZE(message->msg_iov[i].iov_base, message->msg_iov[i].iov_len);
            LOG(DEBUG, "IOV: %p %d\n", message->msg_iov[i].iov_base, message->msg_iov[i].iov_len);
        }
    }

    return 0;
}

/**
 * @brief Socket sendmsg method
 * @param socket The socket file descriptor
 * @param message Contains the message to be sent
 * @param flags Specifies the type of message transmission
 */
ssize_t socket_sendmsg(int socket, struct msghdr *message, int flags) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;
    
    // Is it actually a socket?
    fs_node_t *socknode = FD(current_cpu->current_process, socket)->node;
    if (socknode->flags != VFS_SOCKET) return -ENOTSOCK;

    // Validate the full message
    socket_validateMsg(message);

    sock_t *sock = (sock_t*)socknode->dev;
    if (sock->sendmsg) {
        return sock->sendmsg(sock, message, flags);
    } else {
        return -EINVAL;
    }
}

/**
 * @brief Socket recvmsg method
 * @param socket The socket file descriptor
 * @param message Contains the message buffer to store received one in
 * @param flags Specifies the type of message receive
 */
ssize_t socket_recvmsg(int socket, struct msghdr *message, int flags) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;
    
    // Is it actually a socket?
    fs_node_t *socknode = FD(current_cpu->current_process, socket)->node;
    if (socknode->flags != VFS_SOCKET) return -ENOTSOCK;

    // Validate the full message
    socket_validateMsg(message);

    sock_t *sock = (sock_t*)socknode->dev;
    if (sock->recvmsg) {
        return sock->recvmsg(sock, message, flags);
    } else {
        return -EINVAL;
    }
}

/**
 * @brief Setsockopt for the @c SOL_SOCKET type of sockets
 * @param sock The socket to set options for
 * @param option_name The option to set
 * @param option_value The value of the option
 * @param option_len The length of the option
 */
static int socket_default_setsockopt(sock_t *sock, int option_name, const void *option_value, socklen_t option_len) {
    // Handle boolean options first
    switch (option_name) {
        case SO_DEBUG:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_DEBUG, option_value);
            if (option_value) LOG(DEBUG, "Debug mode enabled for socket\n");
            return 0;
        case SO_BROADCAST:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_BROADCAST, option_value);
            return 0;
        case SO_REUSEADDR:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_REUSEADDR, option_value);
            return 0;
        case SO_KEEPALIVE:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_KEEPALIVE, option_value);
            return 0;
        case SO_OOBINLINE:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_OOBINLINE, option_value);
            return 0;
        case SO_DONTROUTE:
            SOCKET_CHANGE_FLAG(SOCKET_FLAG_DONTROUTE, option_value);
            return 0;
    }

    // Now handle the other types
    switch (option_name) {
        case SO_RCVBUF:
            LOG(ERR, "Receive buffer not implemented\n");
            return 0;   // Our receive/send buffers are dynamic
        case SO_SNDBUF:
            LOG(ERR, "Send buffer not implemented\n");
            return 0;   // Our receive/send buffers are dynamic
        case SO_BINDTODEVICE:
            const char *device = option_value;
            SYSCALL_VALIDATE_PTR(device);

            // Find the NIC
            nic_t *nic = nic_find((char*)device);
            if (!nic) return -ENOENT;

            // Bind it
            sock->bound_nic = nic;

            // Special case
            if (sock->domain == AF_RAW) {
                if (!nic->raw_sockets) nic->raw_sockets = list_create("raw sockets");  
                list_append(nic->raw_sockets, (void*)sock);
            }

            LOG(DEBUG, "Bound to NIC %s\n", device);

            return 0;
    }

    return -ENOPROTOOPT;
}

/**
 * @brief Socket setsockopt method
 * @param socket The socket file descriptor
 * @param level The protocol level for the option
 * @param option_name The name of the option
 * @param option_value The value of the option
 * @param option_len The length of the option
 */
int socket_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    fs_node_t *socknode = FD(current_cpu->current_process, socket)->node;
    if (socknode->flags != VFS_SOCKET) return -ENOTSOCK;

    sock_t *sock = (sock_t*)socknode->dev;

    // TODO: Replace this with a handler system. This is temporary
    switch (level) {
        case SOL_SOCKET:
            return socket_default_setsockopt(sock, option_name, option_value, option_len);
        default:
            return -ENOPROTOOPT;
    }
}

/**
 * @brief Socket bind method
 * @param socket The socket file descriptor
 * @param addr The address to bind to
 * @param addrlen The address length
 */
int socket_bind(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    fs_node_t *socknode = FD(current_cpu->current_process, socket)->node;
    if (socknode->flags != VFS_SOCKET) return -ENOTSOCK;

    sock_t *sock = (sock_t*)socknode->dev;

    if (sock->bind) {
        return sock->bind(sock, addr, addrlen);
    }

    return -EINVAL;
}

/**
 * @brief Socket close method
 */
void socket_close(fs_node_t *node) {
    sock_t *sock = (sock_t*)node->dev;

    // First, call the socket's dedicated close method
    if (sock->close) sock->close(sock);

    // Now free memory
    spinlock_acquire(sock->recv_lock);
    if (sock->recv_lock) spinlock_destroy(sock->recv_lock);
    if (sock->recv_queue) list_destroy(sock->recv_queue, true);
    if (sock->recv_wait_queue) kfree(sock->recv_wait_queue);
    kfree(sock);
}

/**
 * @brief Socket node ready method
 * @param node The node to check 
 * @param events The events we are looking for
 */
int socket_ready(fs_node_t *node, int events) {
    if (node->flags != VFS_SOCKET) return 0;
    int revents = VFS_EVENT_WRITE;

    sock_t *sock = (sock_t*)node->dev;
    if (sock->recv_queue->length) revents |= VFS_EVENT_READ;

    return revents;
}

/**
 * @brief Create a new socket for a given process
 * @param proc The process to create the socket for
 * @param domain The domain to use while creating
 * @param type The type of the socket
 * @param protocol The protocol to use when creating the socket
 * @returns -ERRNO on error and fd on success
 */
int socket_create(process_t *proc, int domain, int type, int protocol) {
    // Look up the protocol to use
    socket_create_t create = (socket_create_t)hashmap_get(socket_map, (void*)(uintptr_t)domain);
    if (!create) return -EAFNOSUPPORT;

    // Create a socket object
    sock_t *sock = create(type, protocol);
    if (!sock) {
        return -EINVAL;
    }

    // Setup receive lock and queue
    if (!sock->recv_lock) sock->recv_lock = spinlock_create("receive lock");
    if (!sock->recv_wait_queue) sock->recv_wait_queue = sleep_createQueue("receive sleep queue");
    if (!sock->recv_queue) sock->recv_queue = list_create("receive queue");

    // Build a filesystem node
    if (!sock->node) {
        fs_node_t *node = kzalloc(sizeof(fs_node_t));
        strcpy(node->name, "socket");
        node->flags = VFS_SOCKET;
        node->atime = node->ctime = node->mtime = now();
        node->dev = (void*)sock;
        node->ready = socket_ready;
        node->close = socket_close;
        sock->node = node;
    }

    // Setup other parameters
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;

    sock->id = last_socket_id++;
    list_append(socket_list, (void*)sock);

    // Add as file descriptor
    fd_t *fd = fd_add(proc, sock->node);
    return fd->fd_number;
}

/**
 * @brief Wait for received content to be available in a socket
 * @param sock The socket to wait on received content for
 * @returns 0 on success, 1 on interrupted
 */
int socket_waitForContent(sock_t *sock) {
    if (sock->recv_queue->length) return 0;

    // Just sleep in the queue, we'll be woken up eventually
    int r = sleep_inQueue(sock->recv_wait_queue);
    return r;
}

/**
 * @brief Write a packet to a socket and alert those who are waiting on it
 * @param sock The socket to write data to
 * @param data The data to write to the socket
 * @param size How much data to write to the socket
 * 
 * The socket can pull this information with @c socket_get 
 */
int socket_received(sock_t *sock, void *data, size_t size) {
    // Get received lock
    spinlock_acquire(sock->recv_lock);

    // Create a new packet
    sock_recv_packet_t *pkt = kzalloc(sizeof(sock_recv_packet_t) + size);
    memcpy(pkt->data, data, size);
    pkt->size = size;

    // Push it into the list
    list_append(sock->recv_queue, (void*)pkt);

    // Alert
    fs_alert(sock->node, VFS_EVENT_READ | VFS_EVENT_WRITE);

    // Wakeup a thread from the queue
    LOG(DEBUG, "Waking up a single thread from queue - packet received\n");
    sleep_wakeupQueue(sock->recv_wait_queue, 1);

    spinlock_release(sock->recv_lock);

    return 0;
}

/**
 * @brief Wait for and get received packets from a socket
 * @param sock The socket to wait on packets from
 * @returns A packet on success, NULL on failure (if NULL assume -EINTR)
 * 
 * @note You are expected to free this packet
 */
sock_recv_packet_t *socket_get(sock_t *sock) {
    // Wait for available socket data
    if (socket_waitForContent(sock)) return NULL;

    spinlock_acquire(sock->recv_lock);
    node_t *n = list_popleft(sock->recv_queue);

    if (!n) {
        // !!!: wtf?
        spinlock_release(sock->recv_lock);
        return NULL;
    }

    sock_recv_packet_t *pkt = (sock_recv_packet_t*)n->value;

    spinlock_release(sock->recv_lock);
    return pkt;
}

/**
 * @brief Get a socket by its ID
 * @param id The ID to look for
 * @returns The socket or NULL
 * @warning This can be kinda slow since it searches the full socket list
 */
sock_t *socket_fromID(int id) {
    // !!!: Slow
    foreach(sock_node, socket_list) {
        sock_t *sock = (sock_t*)sock_node->value;
        if (sock) {
            if (sock->id == id) return sock;
        }
    }
    
    return NULL;
}
