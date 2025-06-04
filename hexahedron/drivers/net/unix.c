/**
 * @file hexahedron/drivers/net/unix.c
 * @brief UNIX socket driver
 * 
 * UNIX sockets can either operate in datagram mode or streamed mode.
 * In streamed mode, Hexahedron uses its own packet types (DATA/ACK/CLOSE/ACCEPT) and packet indexes
 * to ensure ordered transfer.
 * 
 * Every packet sent other than a type of ACK must be acknowledged by the sender, else it will time out and be dropped.
 * 
 * Datagram sockets do not follow this protocol, and Hexahedron does not care whether the data gets there or not.
 * 
 * Do note that Hexahedron does not comply very well with the "sockets as files" things, and reading a socket file will treat
 * it as an actual file. I do not care enough to fix this (nor do I actually know enough about UNIX sockets to determine the best way to)
 * 
 * @warning There can be a lot of race conditions here
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
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <sys/un.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

/* Require type of socket */
#define UNIX_IS_TYPE(t) (((unix_sock_t*)sock->driver)->type == t)

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
 * @brief Acknowledge a packet from an ordered UNIX socket
 * @param sock The UNIX socket to acknowledge on
 * @param packet The packet to acknowledge
 * @returns 0 on success
 */
int unix_acknowledge(sock_t *sock, unix_ordered_packet_t *packet) {
    // Generate an ACK packet and send it
    unix_ordered_packet_t *ack = kmalloc(sizeof(unix_ordered_packet_t));
    ack->type = UNIX_PACKET_TYPE_ACK;
    ack->size = sizeof(unix_ordered_packet_t);
    ack->pkt_idx = packet->pkt_idx;

    int r = unix_sendPacket(sock, ack, ack->size);
    kfree(ack);
    return r;
}

/**
 * @brief Resolve type
 */
char *unix_resolveType(int type) {
    switch (type) {
        case UNIX_PACKET_TYPE_ACCEPT:
            return "ACCEPT";
        case UNIX_PACKET_TYPE_ACK:
            return "ACK";
        case UNIX_PACKET_TYPE_CLOSE:
            return "CLOSE";
        case UNIX_PACKET_TYPE_DATA:
            return "DATA";
        default:
            return "???";
    }
}

/**
 * @brief Read a packet from a UNIX socket
 * @param sock The UNIX socket to read from
 * @returns Received packet
 */
sock_recv_packet_t *unix_getPacket(sock_t *sock) {
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    
    while (1) {
        // We need to read (and possibly acknowledge) the packet
        // Sleep until new data is available
        sock_recv_packet_t *recv = socket_get(sock);
        if (!recv) return NULL;

        // Are we an ordered socket?
        if (sock->type == SOCK_STREAM || sock->type == SOCK_SEQPACKET) {
            // Yes, we need to ACK this packet back (probably)
            unix_ordered_packet_t *upkt = (unix_ordered_packet_t*)recv->data;
            LOG(DEBUG, "[RECV/%s] %p %s <- %s\n", unix_resolveType(upkt->type), upkt, usock->sun_path, ((unix_sock_t*)usock->connected_socket->driver)->sun_path);
            if (upkt->type != UNIX_PACKET_TYPE_ACK && upkt->type != UNIX_PACKET_TYPE_CLOSE) unix_acknowledge(sock, upkt);
        }

        return recv;
    }

}

/**
 * @brief Send a packet to a connected UNIX socket
 * @param sock The UNIX packet to send on
 * @param packet The packet to send
 * @param size The size of the packet
 * @returns 0 on success (for ordered will block until ACK if needed)
 */
int unix_sendPacket(sock_t *sock, void *packet, size_t size) {
    unix_sock_t *usock = (unix_sock_t*)sock->driver;
    unix_sock_t *target = (unix_sock_t*)usock->connected_socket->driver;

    socket_received(usock->connected_socket, packet, size);

    if (sock->type == SOCK_STREAM) {
        // We need to wait for an ACK if this packet wasn't one
        unix_ordered_packet_t *sent_pkt = (unix_ordered_packet_t*)packet;
        LOG(DEBUG, "[SEND/%s] %p %s -> %s\n", unix_resolveType(sent_pkt->type), sent_pkt, usock->sun_path, target->sun_path);
        if (sent_pkt->type != UNIX_PACKET_TYPE_ACK) {
            // No, it wasn't
            sock_recv_packet_t *recv = unix_getPacket(sock);
            if (!recv) return 1;

            unix_ordered_packet_t *ack = (unix_ordered_packet_t*)recv->data;
            if (ack->type != UNIX_PACKET_TYPE_ACK)  {

                // !!!: If two sockets try to close at the same time, all hell breaks lose. There's other hacky checks but this is
                // !!!: just to silence the error log
                if (ack->type != UNIX_PACKET_TYPE_CLOSE) LOG(ERR, "Acknowledgement error on UNIX socket (%s): This packet is of type %d and is not an expected ACK\n", usock->sun_path, ack->type);
                kfree(recv);
                return 1;
            }

            if (ack->pkt_idx != sent_pkt->pkt_idx) {
                LOG(ERR, "Invalid ACK: Expected an ACK for %d but got one for %d\n", sent_pkt->pkt_idx, ack->pkt_idx);
                kfree(recv);
                return 1;
            }

            // Yup, we got an ACK!
            usock->packet_index++;
            kfree(recv);
        }
    }

    return 0;
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

    struct sockaddr_un *un = NULL;

    // Do we accept msg_name?
    if (sock->type == SOCK_DGRAM) {
        // Yeah, but did they give one?
        if (msg->msg_name) {
            if (msg->msg_namelen < sizeof(struct sockaddr_un)) return -EINVAL;
            un = (struct sockaddr_un*)msg->msg_name;
        }
    }

    if (!un) {
        if (!usock->connected_socket) return -ENOTCONN;
        struct sockaddr_un temp;
        temp.sun_family = AF_UNIX;
        strncpy(temp.sun_path, ((unix_sock_t*)usock->connected_socket->driver)->sun_path, 108);
        un = &temp;
    }

    if (!sock->recv_queue->length && sock->flags & SOCKET_FLAG_NONBLOCKING) return -EWOULDBLOCK;

    ssize_t total_received = 0;
    if (sock->type == SOCK_DGRAM) {
        for (int i = 0; i < msg->msg_iovlen; i++) {
            // Preserve message boundaries
            return -ENOTSUP;
        }
    } else {
        if (msg->msg_iovlen > 1) {
            LOG(ERR, "More than one iovec is not currently supported (KERNEL BUG)\n");
            return -ENOTSUP;
        }

        // TODO: This isn't valid. We need to split the data across the iovecs,
        // TODO: not put one packet in one iovec.
        for (int i = 0; i < msg->msg_iovlen; i++) {
            // Get the packet and copy it in
            sock_recv_packet_t *recv = unix_getPacket(sock);
            if (!recv) return -EINTR;
            unix_ordered_packet_t *pkt = (unix_ordered_packet_t*)recv->data;
            
            while (pkt->type != UNIX_PACKET_TYPE_DATA) {
                kfree(recv);
                recv = unix_getPacket(sock);
                pkt = (unix_ordered_packet_t*)recv->data;
                if (pkt->type == UNIX_PACKET_TYPE_CLOSE) {
                    kfree(recv);
                    return -ECONNABORTED;
                }
            }

            // Copy in data
            size_t actual_size = pkt->size - sizeof(unix_ordered_packet_t);
  
            if (actual_size > msg->msg_iov[i].iov_len) {
                // TODO: Set MSG_TRUNC and store this data to be reread
                LOG(WARN, "Truncating packet from %d -> %d\n", actual_size, msg->msg_iov[i].iov_len);
                memcpy(msg->msg_iov[i].iov_base, pkt->data, msg->msg_iov[i].iov_len);
                total_received += msg->msg_iov[i].iov_len;
                kfree(recv);
                continue;
            }

            // Copy it
            memcpy(msg->msg_iov[i].iov_base, pkt->data, actual_size);
            total_received += actual_size;
            kfree(recv);
        }
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

    // Do we accept msg_name?
    if (sock->type == SOCK_DGRAM) {
        // Yeah, but did they give one?
        if (msg->msg_name) {
            if (msg->msg_namelen < sizeof(struct sockaddr_un)) return -EINVAL;
            un = (struct sockaddr_un*)msg->msg_name;
        }
    }

    if (!un) {
        if (!usock->connected_socket) return -ENOTCONN;
        struct sockaddr_un temp;
        temp.sun_family = AF_UNIX;
        strncpy(temp.sun_path, ((unix_sock_t*)usock->connected_socket->driver)->sun_path, 108);
        un = &temp;
    }



    ssize_t total_sent = 0;

    if (sock->type == SOCK_DGRAM) {
        // DGRAM sockets are easy, just throw a packet at them
        for (int i = 0; i < msg->msg_iovlen; i++) {
            unix_unordered_packet_t *pkt = kmalloc(sizeof(unix_unordered_packet_t) + msg->msg_iov[i].iov_len);
            pkt->un.sun_family = AF_UNIX;
            strncpy(pkt->un.sun_path, usock->sun_path, 108);
            pkt->size = sizeof(unix_unordered_packet_t) + msg->msg_iov[i].iov_len;
            memcpy(pkt->data, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
            unix_sendPacket(usock->connected_socket, pkt, pkt->size);
            total_sent += msg->msg_iov[i].iov_len;
        }
    } else {
        // STREAM sockets are not easy, because they don't preserve message boundaries.
        // However, that's all client-side (recvmsg). We can just send each iovec and then have client reassemble them :D
        for (int i = 0; i < msg->msg_iovlen; i++) {
            unix_ordered_packet_t *pkt = kmalloc(sizeof(unix_ordered_packet_t) + msg->msg_iov[i].iov_len);
            pkt->type = UNIX_PACKET_TYPE_DATA;
            pkt->pkt_idx = usock->packet_index;
            pkt->size = sizeof(unix_ordered_packet_t) + msg->msg_iov[i].iov_len;
            memcpy(pkt->data, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);

            if (unix_sendPacket(sock, pkt, pkt->size)) {
                kfree(pkt);
                return -ECONNRESET;
            }

            kfree(pkt);
            total_sent += msg->msg_iov[i].iov_len;
        }

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
    if (!serv->incoming_connections) return -ECONNREFUSED; // Server has not issued a call to listen()
    if (!(sock->type == serv_sock->type) && !(sock->type == SOCK_SEQPACKET && serv_sock->type == SOCK_STREAM) && !(sock->type == SOCK_STREAM && serv_sock->type == SOCK_SEQPACKET)) return -EPROTOTYPE;

    // Create a pending connection
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
            
            if (w == WAKEUP_TIME || !sock->recv_queue || !sock->recv_queue->length) {
                sleep_untilTime(current_cpu->current_thread, 1, 0);
                fs_wait(sock->node, VFS_EVENT_READ);
                continue;
            }
            
            // A thread must've woke us up, read the ordered packet.
            sock_recv_packet_t *recv = socket_get(sock);
            unix_ordered_packet_t *pkt = (unix_ordered_packet_t*)recv->data;

            if (pkt->type == UNIX_PACKET_TYPE_ACCEPT) {
                // ACCEPT packet, let's get the socket we need to connect to and ACK this packet.
                uintptr_t data[1] = { 0x0 };
                memcpy(data, pkt->data, sizeof(uintptr_t));
                sock_t *new_sock = (sock_t*)data[0];
                usock->connected_socket = new_sock;
                

                unix_acknowledge(sock, pkt);
                kfree(recv);
                return 0;
            }

            // Anything else indicates connection failure
            kfree(recv);
            return -ECONNREFUSED;
        }

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

    // We've got a connection request, let's send back an ACCEPT event
    unix_ordered_packet_t *pkt = kzalloc(sizeof(unix_ordered_packet_t) + sizeof(void*));
    pkt->type = UNIX_PACKET_TYPE_ACCEPT;
    pkt->size = sizeof(unix_ordered_packet_t) + sizeof(void*);
    pkt->pkt_idx = usock->packet_index;
    
    uintptr_t data[1] = { (uintptr_t)new_sock };
    memcpy((void*)pkt->data, data, sizeof(uintptr_t));

    if (unix_sendPacket(new_sock, pkt, pkt->size)) {
        kfree(creq);
        kfree(pkt);
        fs_close(new_sock->node);
        return -ECONNABORTED;
    }

    kfree(pkt);
    
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
    kfree(creq);
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
        // TODO: Reply with a CLOSE (?)
        list_destroy(usock->incoming_connections, false); 
        usock->incoming_connections = NULL;
    }

    if (usock->connected_socket) {
        sock_t *prev_sock = usock->connected_socket;
        ((unix_sock_t*)usock->connected_socket->driver)->connected_socket = NULL;
    
        // Make a CLOSED packet
        unix_ordered_packet_t close = {
            .type = UNIX_PACKET_TYPE_CLOSE,
            .pkt_idx = usock->packet_index,
            .size = sizeof(unix_ordered_packet_t)
        };

        socket_received(prev_sock, (void*)&close, sizeof(unix_ordered_packet_t));
    }

    if (usock->bound) fs_close(usock->bound);
    
    // TODO: Any other things? There are a lot of race conditions...
    kfree(usock);
    return 0;
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

    sock->sendmsg = unix_sendmsg;
    sock->recvmsg = unix_recvmsg;
    sock->close = unix_close;
    sock->connect = unix_connect;
    sock->bind = unix_bind;
    sock->listen = unix_listen;
    sock->accept = unix_accept;

    sock->driver = (void*)usock;

    return sock;
}
