/**
 * @file hexahedron/drivers/net/unix.c
 * @brief UNIX socket driver
 * 
 * UNIX sockets can either operate in datagram mode or streamed mode.
 * In either mode, Hexahedron uses a circular buffer (combined with a datagram data system) that ensures reliable
 * transmission.
 * 
 * Thank you to Stanislas Orsola (@tayoky4848) for helping me see the light in pipe implementations.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/net/unix.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/misc/spinlock.h>
#include <kernel/task/syscall.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <sys/un.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NET:UNIX", __VA_ARGS__)

/* UNIX mount hashmap */
hashmap_t *unix_mount_map = NULL;

/* UNIX mount lock */
spinlock_t unix_mount_lock = { 0 };

/* Socket close method */
extern void socket_close(fs_node_t *node);

/**
 * @brief Initialize the UNIX socket system
 */
void unix_init() {
    unix_mount_map = hashmap_create("unix mount map", 20);
    socket_register(AF_UNIX, unix_socket);
}

/**
 * @brief UNIX socket recvmsg method
 * @param sock The socket to receive messages on
 * @param msg The message to receive
 * @param flags Additional flags
 */
ssize_t unix_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;

    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    ssize_t total_received = 0;
    if (sock->type == SOCK_DGRAM) {
        if (msg->msg_iovlen > 1) {
            LOG(ERR, "Multiple iovecs are not supported right now\n");
            return -ENOTSUP;
        }

        // Datagram sockets are easy enough, we just need to also pop from the datagram data queue
        // Reuse the lock from the ringbuffer
        spinlock_acquire(usock->packet_buffer->lock);
        if (!usock->dgram_data->length) {
            if (sock->flags & SOCKET_FLAG_NONBLOCKING) {
                spinlock_release(usock->packet_buffer->lock);
                return -EWOULDBLOCK;
            }

            // There is no length, keep holding the lock and prepare to sleep
            sleep_untilNever(current_cpu->current_thread);
            usock->thr = current_cpu->current_thread;
            
            spinlock_release(usock->packet_buffer->lock);
            
            int w = sleep_enter();
            if (w == WAKEUP_SIGNAL) return -EINTR;

            // Another thread must've woken us up, reacquire the lock
            spinlock_acquire(usock->packet_buffer->lock);
        }

        // We have the lock and there's content available.
        // Pop the node
        node_t *n = list_popleft(usock->dgram_data);
        if (!n) return -EINTR;  // ???
        unix_datagram_data_t *data = (unix_datagram_data_t*)n->value;
        kfree(n);

        // We have the size of the packet. Which is less?
        size_t size_to_read = min(msg->msg_iov[0].iov_len, data->packet_size);
        
        // Read from the buffer
        ssize_t r = circbuf_read(usock->packet_buffer, size_to_read, msg->msg_iov[0].iov_base);
        if (r && msg->msg_name && msg->msg_namelen == sizeof(struct sockaddr_un)) {
            // We got a message name as well
            memcpy(msg->msg_name, &data->un, sizeof(struct sockaddr_un));
        }

        kfree(data);
        return r;
    } else {
        if (msg->msg_iovlen > 1) {
            LOG(ERR, "More than one iovec is not currently supported (KERNEL BUG)\n");
            return -ENOTSUP;
        }

        // Streamed sockets are easy, just try to read as much as possible
        LOG(INFO, "Now waiting for data\n");
        if (usock->packet_buffer->stop && !circbuf_remaining_read(usock->packet_buffer)) return -ECONNRESET;
        ssize_t r = 0;
        
        while (!r) {
            r = circbuf_read(usock->packet_buffer, msg->msg_iov[0].iov_len, (uint8_t*)msg->msg_iov[0].iov_base);
        }
        
        return r;
    }

    return total_received;
}

/**
 * @brief UNIX socket sendmsg method
 * @param sock The socket to send the message on
 * @param msg The message to send
 * @param flags Additional flags
 */
ssize_t unix_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;

    unix_sock_t *usock = (unix_sock_t*)sock->driver;

    struct sockaddr_un *un = NULL;
    sock_t *chosen_socket = NULL;

    // Do we accept msg_name?
    if (sock->type == SOCK_DGRAM) {
        // Yeah, but did they give one?
        if (msg->msg_name) {
            if (msg->msg_namelen < sizeof(struct sockaddr_un)) return -EINVAL;
            un = (struct sockaddr_un*)msg->msg_name;
        
            // Lookup the path they're trying to send to
            // Canonicalize the path
            char *canon = vfs_canonicalizePath(current_cpu->current_process->wd_path, un->sun_path);
            if (!canon) return -EINVAL;

            // Try to get the node at the address
            sock_t *serv_sock = hashmap_get(unix_mount_map, canon);
            if (!serv_sock) return -ENOTSOCK; // !!!: ENOENT?

            // Double-check type
            if (serv_sock->type != SOCK_DGRAM) return -EPROTOTYPE;
        
            // Chosen socket
            chosen_socket = serv_sock;
            LOG(DEBUG, "Resolved address for \"%s\"\n", un->sun_path);
        } else {
            if (!usock->connected_socket) return -ENOTCONN;
            chosen_socket = usock->connected_socket;
        }
    } else {
        if (!usock->connected_socket) return -ENOTCONN;
        chosen_socket = usock->connected_socket;
    }

    ssize_t total_sent = 0;

    if (sock->type == SOCK_DGRAM) {
        // DGRAM sockets are easy, just throw a packet at them
        if (msg->msg_iovlen > 1) {
            LOG(ERR, "Multiple IOVs not supported yet!\n");
            return 1;
        }

        unix_sock_t *usock_target = (unix_sock_t*)chosen_socket->driver;
        
        // Create packet data
        unix_datagram_data_t *data = kmalloc(sizeof(unix_datagram_data_t));
        data->packet_size = msg->msg_iov[0].iov_len;
        data->un.sun_family = AF_UNIX;
        memcpy(data->un.sun_path, usock->sun_path, 108);
 
        // Write to circular buffer
        ssize_t r = circbuf_write(usock_target->packet_buffer, msg->msg_iov[0].iov_len, msg->msg_iov[0].iov_base);
        if (r) {
            // Acquire lock to push packet data
            spinlock_acquire(usock_target->packet_buffer->lock);
            list_append(usock_target->dgram_data, data);
            spinlock_release(usock_target->packet_buffer->lock);

            if (usock_target->thr) sleep_wakeup(usock_target->thr);
        }

        return r;
    } else {
        if (msg->msg_iovlen > 1) {
            LOG(ERR, "Multiple IOVs not supported yet!\n");
            return 1;
        }

        unix_sock_t *usock_target = (unix_sock_t*)chosen_socket->driver;

        // Streamed sockets are even easier, just write the packet for them.
        ssize_t r = circbuf_write(usock_target->packet_buffer, msg->msg_iov[0].iov_len, (uint8_t*)msg->msg_iov[0].iov_base);
        if (!r && usock_target->packet_buffer->stop) return -ECONNRESET;
        return r;
    }
    
    return total_sent;
}

/**
 * @brief UNIX socket connect method
 * @param sock The socket to connect
 * @param sockaddr The socket address
 * @param addrlen The address length
 */
int unix_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    // Are we already connected?
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    if (usock->connected_socket) {
        return -EISCONN;
    }

    if (addrlen != sizeof(struct sockaddr_un)) return -EINVAL;
    struct sockaddr_un *addr = (struct sockaddr_un*)sockaddr;

    // Canonicalize the path
    char *canon = vfs_canonicalizePath(current_cpu->current_process->wd_path, addr->sun_path);
    if (!canon) return -EINVAL;

    // Try to get the node at the address
    sock_t *serv_sock = hashmap_get(unix_mount_map, canon);
    if (!serv_sock) return -ENOTSOCK; // !!!: ENOENT?

    // Is it the same type as us?
    unix_sock_t *serv = (unix_sock_t*)serv_sock->driver;
    if (!(sock->type == serv_sock->type) && !(sock->type == SOCK_SEQPACKET && serv_sock->type == SOCK_STREAM) && !(sock->type == SOCK_STREAM && serv_sock->type == SOCK_SEQPACKET)) {
        LOG(WARN, "Cannot connect to socket of type %d when you are type %d\n", serv_sock->type, sock->type);
        return -EPROTOTYPE;
    }

    // If we are a datagram socket, stop now
    if (sock->type == SOCK_DGRAM) {
        usock->connected_socket = serv_sock;
        return 0;
    }

    // Create a pending connection
    if (!serv->incoming_connections) return -ECONNREFUSED; // Server has not issued a call to listen()

    // TODO: Handle backlog
    unix_conn_request_t *creq = kzalloc(sizeof(unix_conn_request_t));
    creq->sock = sock;

    // Add ourselves to the incoming connections queue
    spinlock_acquire(&serv->incoming_connect_lock);
    list_append(serv->incoming_connections, (void*)creq);
    spinlock_release(&serv->incoming_connect_lock);

    // Prepare ourselves if needed
    if (sock->type == SOCK_STREAM) {
        sleep_untilTime(current_cpu->current_thread, 1, 0);
        fs_wait(sock->node, VFS_EVENT_READ);
        usock->thr = current_cpu->current_thread;
    }

    if (serv->thr && serv->thr->sleep) sleep_wakeup(serv->thr);
    
    // Do we need to wait for an acknowledgement?
    if (sock->type == SOCK_DGRAM) {
        // Nope, assume that we're bound and set the connected socket.
        usock->connected_socket = serv_sock; // ???: wasting these extra 8 bytes is probably faster than connected_addr
    } else {
        // Yes, we need to wait for an ACCEPT request.
        // Start waiting in timeout
        for (int i = 0; i < 3; i++) {
            int w = sleep_enter();
            if (w == WAKEUP_SIGNAL) {
                return -EINTR;
            }
            
            // Is it accepted?
            if (creq->new_sock) {
                LOG(INFO, "Connection request accepted\n");
                usock->connected_socket = creq->new_sock;        
                kfree(creq);
                return 0;
            }

            sleep_untilTime(current_cpu->current_thread, 1, 0);
            fs_wait(sock->node, VFS_EVENT_READ);
        }

        kfree(creq);
        return -ETIMEDOUT;
    }

    return 0;
}

/**
 * @brief UNIX socket bind method
 * @param sock The socket to bind
 * @param sockaddr The socket address to bind to
 * @param addrlen The address length
 */
int unix_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    // Are we already bound?
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    if (usock->bound) {
        return -EINVAL;
    }

    // Is it a UNIX socket address?
    if (addrlen != sizeof(struct sockaddr_un)) return -EINVAL;
    struct sockaddr_un *addr = (struct sockaddr_un*)sockaddr;
    if (!(*addr->sun_path)) return -EINVAL;

    // Canonicalize the path
    char *path = vfs_canonicalizePath(current_cpu->current_process->wd_path, addr->sun_path);
    if (!path) return -EINVAL;

    // Try to create the file
    fs_node_t *node = NULL;
    int r = vfs_creat(&node, path, 0);
    if (r != 0) {
        if (r == -EEXIST) return -EADDRINUSE;
        return r;
    }

    // We've created the file successfully, set it to a VFS socket
    // !!!: I have no idea the purpose of this node in UNIX sockets. Is it a semaphore? Can you read from it? Please enlighten me.
    node->flags = VFS_SOCKET;
    node->read = NULL;
    node->write = NULL;

    // We've bound successfully, set it in the map
    usock->bound = node;

    spinlock_acquire(&unix_mount_lock);
    hashmap_set(unix_mount_map, path, sock);
    spinlock_release(&unix_mount_lock);
    
    usock->map_path = path;

    strncpy(usock->sun_path, addr->sun_path, 108);

    LOG(DEBUG, "Bound socket to %s\n", addr->sun_path);

    return 0;
}

/**
 * @brief UNIX socket listen method
 * @param sock The socket to listen on
 * @param backlog The backlog to use
 */
int unix_listen(sock_t *sock, int backlog) {
    // Create incoming connections
    if (sock->type == SOCK_DGRAM) return -EOPNOTSUPP;

    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    if (usock->connected_socket) return -EINVAL;
    if (!usock->incoming_connections) usock->incoming_connections = list_create("unix socket incoming connections");
    return 0;
}

/**
 * @brief UNIX socket accept method
 * @param sock The socket to accept on
 * @param sockaddr Output address if required
 * @param addrlen Address length
 */
int unix_accept(sock_t *sock, struct sockaddr *sockaddr, socklen_t *addrlen) {
    // Is this socket bound?
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    if (!usock->incoming_connections) return -EINVAL;
    if (!usock->bound) return -EINVAL;

    // Is it a datagram socket?
    if (sock->type == SOCK_DGRAM) return -EOPNOTSUPP;

    unix_conn_request_t *creq = NULL;

    while (1) {
        if (!usock->incoming_connections->length) {
            if (sock->flags & SOCKET_FLAG_NONBLOCKING) return -EWOULDBLOCK;
            
            // Wait for a connection event  
            sleep_untilNever(current_cpu->current_thread);
            usock->thr = current_cpu->current_thread;

            int w = sleep_enter();

            // Why were we woken up?
            if (w == WAKEUP_SIGNAL) return -EINTR;
            if (!usock->incoming_connections) return -ECONNABORTED; // Just in case we close?
        }

        // Another thread woke us up, why?
        spinlock_acquire(&usock->incoming_connect_lock);
        if (!usock->incoming_connections->length) {
            // Nope
            spinlock_release(&usock->incoming_connect_lock);
            continue;
        } 

        node_t *n = list_popleft(usock->incoming_connections);
        creq = (unix_conn_request_t*)n->value;
        spinlock_release(&usock->incoming_connect_lock);

        kfree(n);
        break;
    }

    // Make a new socket to receive on
    int sock_fd = socket_create(current_cpu->current_process, AF_UNIX, sock->type, sock->protocol);
    if (sock_fd < 0) {
        kfree(creq);
        return -ECONNABORTED;
    }

    sock_t *new_sock = (sock_t*)FD(current_cpu->current_process, sock_fd)->node->dev;

    // Got a new socket successfully, say that we're bound already
    unix_sock_t *new_usock = (unix_sock_t*)new_sock->driver;
    new_usock->connected_socket = creq->sock;
    strncpy(new_usock->sun_path, usock->sun_path, 108);

    // Set the new socket
    creq->new_sock = new_sock;
    
    // Wakeup the waiting socket thread
    sleep_wakeup(((unix_sock_t*)creq->sock->driver)->thr);

    // Fill in accept info if we can
    if (addrlen) {
        size_t size = (*addrlen > sizeof(struct sockaddr_un)) ? sizeof(struct sockaddr_un) : *addrlen;
        SYSCALL_VALIDATE_PTR_SIZE(sockaddr, *addrlen);

        struct sockaddr_un new_un;
        new_un.sun_family = AF_UNIX;
        strncpy(new_un.sun_path, usock->sun_path, 108);

        memcpy(sockaddr, &new_un, size);
    }

    // We are connected
    return sock_fd;
}

/**
 * @brief UNIX socket close method
 * @param sock The socket to close
 */
int unix_close(sock_t *sock) {
    unix_sock_t *usock = (unix_sock_t*)sock->driver;

    // !!!: Race condition?
    if (usock->incoming_connections) {
        // TODO: Reply with a close?
        list_destroy(usock->incoming_connections, false); 
        usock->incoming_connections = NULL;
    }

    // Stop the other socket's buffer
    if (usock->connected_socket && sock->type != SOCK_DGRAM) {
        unix_sock_t *other_sock = (unix_sock_t*)usock->connected_socket->driver;
        circbuf_stop(other_sock->packet_buffer);
    }

    // Drop from hashmap
    spinlock_acquire(&unix_mount_lock);
    hashmap_remove(unix_mount_map, usock->map_path);
    spinlock_release(&unix_mount_lock);

    // Close bound node
    if (usock->bound) fs_close(usock->bound);

    if (sock->type == SOCK_DGRAM) list_destroy(usock->dgram_data, true);
    circbuf_destroy(usock->packet_buffer);
    kfree(usock);
    return 0;
}

/**
 * @brief UNIX socket ready method
 * @param sock The socket
 * @param events Events to check for
 */
int unix_ready(sock_t *sock, int events) {
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    int revents = (circbuf_available(usock->packet_buffer) ? VFS_EVENT_READ : 0) | VFS_EVENT_WRITE;
    return revents;
}

/**
 * @brief Create a UNIX socket
 * @param type The type
 * @param protocol The protocol
 */
sock_t *unix_socket(int type, int protocol) {
    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) return NULL;

    sock_t *sock = kzalloc(sizeof(sock_t));
    unix_sock_t *usock = kzalloc(sizeof(unix_sock_t));

    usock->packet_buffer = circbuf_create("unix packet buffer", UNIX_PACKET_BUFFER_SIZE);
    if (type == SOCK_DGRAM) usock->dgram_data = list_create("unix packet sizes");

    sock->sendmsg = unix_sendmsg;
    sock->recvmsg = unix_recvmsg;
    sock->close = unix_close;
    sock->connect = unix_connect;
    sock->bind = unix_bind;
    sock->listen = unix_listen;
    sock->accept = unix_accept;
    sock->ready = unix_ready;

    sock->driver = (void*)usock;

    return sock;
}
