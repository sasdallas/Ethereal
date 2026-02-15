/**
 * @file hexahedron/drivers/net/socket.c
 * @brief Socket handler
 * 
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
#include <kernel/drivers/net/socket_fs.h>
#include <kernel/task/syscall.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <string.h>
#include <structs/hashmap.h>
#include <errno.h>
#include <sys/ioctl.h>

/* Handler map */
hashmap_t *socket_map = NULL;

/* Socket list */
list_t *socket_list = NULL;

/* Last socket ID */
int last_socket_id = 0;

/* Validate socket option */
#define SOCKET_VALIDATE_OPT(optvalue, optlen) SYSCALL_VALIDATE_PTR_SIZE(optvalue, optlen)

/* Set/get socket option in flags */
#define SOCKET_CHECK_LEN(len, needed) { if (*len < sizeof(needed)) { return 0; }; }
#define SOCKET_CHANGE_FLAG(flag, value) { if (value) { sock->flags |= flag; } else { sock->flags &= ~(flag); } };
#define SOCKET_GET_FLAG(flag, value, len) { if (sock->flags & flag) { *(int*)value = 1; } else { *(int*)value = 0; }; if (*len > sizeof(int)) *len = sizeof(int); }

/* Socket cache */
slab_cache_t *socket_cache;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NET:SOCKET", __VA_ARGS__)

/**
 * @brief Allocate a socket
 * @param domain The domain of the socket
 * @param type The type of the socket
 * @param protocol The protocol of the socket
 */
sock_t *socket_allocate(int domain, int type, int protocol) {
    sock_t *r = slab_allocate(socket_cache);
    if (!r) return NULL;

    memset(r, 0, sizeof(sock_t));

    r->domain = domain;
    r->type = type;
    r->protocol = protocol;

    return r;
}

/**
 * @brief Free a socket
 */
void socket_free(sock_t *sock) {
    slab_free(socket_cache, sock);
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
    return 0;

    SYSCALL_VALIDATE_PTR_SIZE(message, sizeof(struct msghdr));
    
    if (message->msg_control) SYSCALL_VALIDATE_PTR_SIZE(message->msg_control, message->msg_controllen);
    if (message->msg_name) SYSCALL_VALIDATE_PTR_SIZE(message->msg_name, message->msg_namelen);

    if (message->msg_iovlen) {
        SYSCALL_VALIDATE_PTR_SIZE(message->msg_iov, message->msg_iovlen * sizeof(struct iovec));

        // For each iov
        for (unsigned i = 0; i < message->msg_iovlen; i++) {
            SYSCALL_VALIDATE_PTR_SIZE(message->msg_iov[i].iov_base, message->msg_iov[i].iov_len);
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
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if (f->inode->attr.type != VFS_SOCKET) return -ENOTSOCK;

    // Validate the full message
    socket_validateMsg(message);

    sock_t *sock = (sock_t*)f->priv;
    if (sock->ops->sendmsg) {
        return sock->ops->sendmsg(sock, message, flags);
    } else {
        return -ENODEV;
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
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if (f->inode->attr.type != VFS_SOCKET) return -ENOTSOCK;

    // Validate the full message
    socket_validateMsg(message);

    sock_t *sock = (sock_t*)f->priv;
    if (sock->ops->recvmsg) {
        return sock->ops->recvmsg(sock, message, flags);
    } else {
        return -ENODEV;
    }
}

/**
 * @brief Socket ioctl method
 * @param node The node to do ioctl() on
 * @param request The requested ioctl()
 * @param argp The argument to ioctl
 */
int socket_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    sock_t *sock = (sock_t*)node->dev;
    switch (request) {
        case FIONBIO:
            SYSCALL_VALIDATE_PTR(argp);
            sock->flags |= (*(int*)argp ? SOCKET_FLAG_NONBLOCKING : 0);
            return 0;
        default:
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
            assert(0 && "SO_BINDTODEVICE");
            return 0;
    }

    return -ENOPROTOOPT;
}


/**
 * @brief Getsockopt for the @c SOL_SOCKET type of sockets
 * @param sock The socket to set options for
 * @param option_name The option to set
 * @param option_value The value of the option
 * @param option_len The length of the option
 */
static int socket_default_getsockopt(sock_t *sock, int option_name, void *option_value, socklen_t *option_len) {
    // Handle boolean options first
    switch (option_name) {
        case SO_DEBUG:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_DEBUG, option_value, option_len);
            return 0;
        case SO_BROADCAST:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_BROADCAST, option_value, option_len);
            return 0;
        case SO_REUSEADDR:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_REUSEADDR, option_value, option_len);
            return 0;
        case SO_KEEPALIVE:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_KEEPALIVE, option_value, option_len);
            return 0;
        case SO_OOBINLINE:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_OOBINLINE, option_value, option_len);
            return 0;
        case SO_DONTROUTE:
            SOCKET_CHECK_LEN(option_len, int);
            SOCKET_GET_FLAG(SOCKET_FLAG_DONTROUTE, option_value, option_len);
            return 0;
    }

    // Other cases
    switch (option_name) {
        case SO_ERROR:
            SOCKET_CHECK_LEN(option_len, int);
            *((int*)option_len) = 0;
            return 0;
    }

    // TODO
    LOG(ERR, "Unimplemented protocol option: %d\n", option_name);
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
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    // TODO: Replace this with a handler system. This is temporary
    switch (level) {
        case SOL_SOCKET:
            return socket_default_setsockopt(sock, option_name, option_value, option_len);
        default:
            return -ENOTSUP;
    }
}

/**
 * @brief Get socket option
 * @param socket The socket file descriptor
 * @param level The protocol level for the option
 * @param option_name The name of the option
 * @param option_value The value of the option
 * @param option_len The length of the option
 */
int socket_getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    SYSCALL_VALIDATE_PTR(option_len);
    SYSCALL_VALIDATE_PTR_SIZE(option_value, *option_len);

    // TODO: Replace this with a handler system. This is temporary
    switch (level) {
        case SOL_SOCKET:
            return socket_default_getsockopt(sock, option_name, option_value, option_len);
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
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if (f->inode->attr.type != VFS_SOCKET) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    SYSCALL_VALIDATE_PTR_SIZE(addr, addrlen);

    if (sock->ops->bind) {
        return sock->ops->bind(sock, addr, addrlen);
    }

    return -ENODEV;
}

/**
 * @brief Socket connect method
 * @param socket The socket file descriptor
 * @param addr The address connect to
 * @param addrlen The address length
 */
int socket_connect(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    SYSCALL_VALIDATE_PTR_SIZE(addr, addrlen);

    if (sock->ops->connect) {
        return sock->ops->connect(sock, addr, addrlen);
    }

    return -ENODEV;
}

/**
 * @brief Socket listen method
 * @param socket The socket file descriptor
 * @param backlog Backlog
 */
int socket_listen(int socket, int backlog) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    if (sock->ops->listen) {
        return sock->ops->listen(sock, backlog);
    }

    return -ENODEV;
}

/**
 * @brief Socket accept method
 * @param socket The socket file descriptor
 * @param addr The address to store the connection in
 * @param addrlen The address length
 */
int socket_accept(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    if (addrlen) SYSCALL_VALIDATE_PTR_SIZE(addr, (*addrlen));

    // !!!: Pointer validation in cases of addrlen = 0?
    if (sock->ops->accept) {
        return sock->ops->accept(sock, addr, addrlen);
    }

    return -ENODEV;
}

/**
 * @brief Socket getpeername method
 * @param socket The socket file descriptor
 * @param address The address to store in
 * @param address_len The address length
 */
int socket_getpeername(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    if (addrlen) SYSCALL_VALIDATE_PTR_SIZE(addr, (*addrlen));
    if (!(*addrlen)) return 0; // TODO: weird case?

    if (sock->ops->getpeername) {
        return sock->ops->getpeername(sock, addr, addrlen);
    }

    return -ENODEV;
}

/**
 * @brief Socket getsockname method
 * @param socket The socket file descriptor
 * @param address The address to store in
 * @param address_len The address length
 */
int socket_getsockname(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    // Lookup the file descriptor
    if (!FD_VALIDATE(current_cpu->current_process, socket)) return -EBADF;

    // Is it actually a socket?
    vfs_file_t *f = FD(current_cpu->current_process, socket)->node;
    if ((f->inode->attr.type != VFS_SOCKET)) return -ENOTSOCK;

    sock_t *sock = (sock_t*)f->priv;

    // ???: Can addrlen be NULL?
    SYSCALL_VALIDATE_PTR(addrlen);
    if (!(*addrlen)) return 0; // TODO: Weird cases?
    SYSCALL_VALIDATE_PTR_SIZE(addr, (*addrlen));

    // !!!: Pointer validation in cases of addrlen = 0?
    if (sock->ops->getsockname) {
        return sock->ops->getsockname(sock, addr, addrlen);
    }

    return -EOPNOTSUPP;
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
    int type_original = type;
    type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

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

    sock->node = (fs_node_t*)0xdeadbeef;
    sock->inode = socketfs_create(sock);

    // Setup other parameters
    sock->domain = domain;
    sock->type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    sock->protocol = protocol;

    sock->id = last_socket_id++;
    list_append(socket_list, (void*)sock);

    if (type_original & SOCK_NONBLOCK) {
        SOCKET_CHANGE_FLAG(SOCKET_FLAG_NONBLOCKING, 1);
    }

    // Add as file descriptor
    vfs_file_t *f;    
    assert(vfs_openat(sock->inode, NULL, O_RDWR, &f) == 0);

    inode_release(sock->inode); // required after creation so that the last reference can be dropped

    fd_t *fd = fd_add(proc, f);
    if (type_original & SOCK_CLOEXEC) {
        LOG(WARN, "SOCK_CLOEXEC is not supported\n");
    }

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
    sleep_inQueue(sock->recv_wait_queue);
    int r = sleep_enter();
    return r == WAKEUP_SIGNAL;
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
    poll_signal(&sock->sock_event, POLLIN);

    // Wakeup a thread from the queue
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
        LOG(ERR, "Error popping from recv queue\n");
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


/**
 * @brief Initialize the socket system
 */
static int socket_init() {
    socket_map = hashmap_create_int("socket map", 4);
    socket_list = list_create("socket list");
    assert((socket_cache = slab_createCache("socket node cache", SLAB_CACHE_DEFAULT, sizeof(sock_t), 0, NULL, NULL)));
    return 0;
}

NET_INIT_ROUTINE(socket, INIT_FLAG_DEFAULT, socket_init);