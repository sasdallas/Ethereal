/**
 * @file hexahedron/drivers/net/unix.c
 * @brief UNIX socket driver
 * 
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
#include <sys/un.h>
#include <kernel/mm/alloc.h>
#include <kernel/fs/vfs.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <kernel/init.h>
#include <structs/hashmap.h>
#include <kernel/debug.h>

/* Path map */
hashmap_t *unix_path_map = NULL;
spinlock_t path_map_lck = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVERS:NET:UNIX", __VA_ARGS__)

/* Get unix socket */
#define USOCK(sock) ((unix_sock_t*)sock->driver)

/* State */
#define UNIX_STATE_CHANGE(usock, s) atomic_store(&usock->state, s)

/**
 * @brief UNIX decrement and free
 */
void unix_decrementAndFree(unix_sock_t *usock) {
    refcount_dec(&usock->ref);
    if (usock->ref) return;
    
    // Destroy the socket
    LOG(INFO, "Need to destroy UNIX socket\n");
    if (usock->peer) usock->peer->peer = NULL;
    if (usock->un_path) kfree(usock->un_path);
    

    if (usock->sock->type == SOCK_STREAM) {
        // TODO
        assert(0 && "unix_decrementAndFree needs implementation for SOCK_STREAM");
    } else {
        // DGRAM/SEQPACKET, destroy
        kfree(usock->pkt.d->rx_wait_queue);
        kfree(usock->pkt.d->tx_wait_queue);
        
        // Go through packet list and free each one
        unix_sock_packet_t *p = usock->pkt.d->rx_head;
        while (p) {
            unix_sock_packet_t *nxt = p->next;
            kfree(p);
            p = nxt;
        }
    }

    if (usock->node) fs_close(usock->node);

    if (usock->is_listener) {
        mutex_destroy(usock->server.m); // No clients should be connected

        while (!queue_empty(usock->server.conn)) {
            unix_conn_req_t *c = queue_pop(usock->server.conn);
            if (c->state == UNIX_CONN_DEAD) free(c);
            else c->state = UNIX_CONN_DEAD;
        }

        kfree(usock->server.conn);
        kfree(usock->server.accepters);
    }    

    kfree(usock);
}

/**
 * @brief UNIX socket bind method
 * @param sock The sock to bind
 * @param addr The address to bind to
 * @param addrlen The address length
 */
int unix_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    unix_sock_t *usock = USOCK(sock);

    if (usock->state != UNIX_SOCK_STATE_INIT) {
        return -EINVAL; // Bound already
    }

    if (addrlen != sizeof(struct sockaddr_un)) return -EINVAL;
    struct sockaddr_un *addr = (struct sockaddr_un*)sockaddr;
    if (!(*addr->sun_path)) return -EINVAL;

    // Canonicalize the current path
    char *p = vfs_canonicalizePath(current_cpu->current_process->wd_path, addr->sun_path);
    assert(p);

    // Try to create the node at the location
    fs_node_t *n = NULL;
    int r = vfs_creat(&n, p, 0);
    if (r != 0) {
        if (r == -EEXIST) return -EADDRINUSE;
        return r;
    }

    // Let's configure the socket
    // TODO: Flush this inode back to disk.. need some more VFS functions!
    // TODO: Maybe more here?
    n->flags = VFS_SOCKET;
    n->read = NULL;
    n->write = NULL;

    usock->node = n;

    spinlock_acquire(&path_map_lck);
    hashmap_set(unix_path_map, p, sock);
    spinlock_release(&path_map_lck);

    usock->un_path = p;
    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_BOUND);

    return 0;
}

/**
 * @brief UNIX socket listen method
 * @param sock The socket to listen on
 * @param backlog The backlog to use
 */
int unix_listen(sock_t *sock, int backlog) {
    if (sock->type != SOCK_SEQPACKET && sock->type != SOCK_STREAM) return -EOPNOTSUPP;

    unix_sock_t *usock = USOCK(sock); 

    if (usock->state == UNIX_SOCK_STATE_LISTEN) {
        return 0; // backlog is not needed
    }

    if (usock->state != UNIX_SOCK_STATE_BOUND) {
        return -EINVAL;
    }
    
    // Create listening structures
    usock->server.m = mutex_create("unix connection mutex");
    usock->server.conn = queue_create();
    usock->server.accepters = sleep_createQueue("unix socket acceptors");

    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_LISTEN);

    return 0;
}

/**
 * @brief UNIX socket accept method
 * @param sock The socket to accept on
 * @param sockaddr Output address if required
 * @param addrlen Address length
 */
int unix_accept(sock_t *sock, struct sockaddr *sockaddr, socklen_t *addrlen) {
    unix_sock_t *u = USOCK(sock);
    if (u->state != UNIX_SOCK_STATE_LISTEN) return -EINVAL;

    // Look for available connection requests
    unix_conn_req_t *r = NULL;
    while (!r) {
        mutex_acquire(u->server.m);

        // Any pending?
        if (!queue_empty(u->server.conn)) {
            // Yes, we do have a pending. Pop it!
            LOG(DEBUG, "Pending connection detected\n");
            r = queue_pop(u->server.conn);
            assert(r);

            if (r->state == UNIX_CONN_DEAD) {
                // Oops
                free(r);
                r = NULL;
            }
        }
    
        if (!r) {
            if (sock_nonblocking(sock)) {
                mutex_release(u->server.m);
                return -EWOULDBLOCK;
            } 

            // Wait in acceptor queue
            sleep_inQueue(u->server.accepters);
            mutex_release(u->server.m);
            int w = sleep_enter();
            if (w == WAKEUP_SIGNAL) return -EINTR;
        } else {
            mutex_release(u->server.m);
        }
    }

    // We should have a pending request, let's build it up
    unix_sock_t *tgt = USOCK(r->socket);
    
    // Create a new peer socket
    int sfd = socket_create(current_cpu->current_process, AF_UNIX, sock->type, sock->protocol);
    if (sfd < 0) {
        // Client should catch itself
        return -ECONNABORTED;
    }

    sock_t *new_sock = (sock_t*)FD(current_cpu->current_process, sfd)->node->dev;
    unix_sock_t *new_usock = USOCK(new_sock);

    refcount_inc(&new_usock->ref);
    refcount_inc(&tgt->ref);
    UNIX_STATE_CHANGE(tgt, UNIX_SOCK_STATE_CONNECTED);
    UNIX_STATE_CHANGE(new_usock, UNIX_SOCK_STATE_CONNECTED);
    


    new_usock->un_path = strdup(u->un_path);

    // Connect up the peers
    tgt->peer = new_usock;
    new_usock->peer = tgt;

    assert(r->state == UNIX_CONN_WAITING);
    atomic_store(&r->state, UNIX_CONN_CONNECTED);
    sleep_wakeup(r->thr);

    // Fillout addrlen if needed
    if (addrlen) {
        size_t size = (*addrlen > sizeof(struct sockaddr_un)) ? sizeof(struct sockaddr_un) : *addrlen;
        SYSCALL_VALIDATE_PTR_SIZE(sockaddr, *addrlen);
    
        
        struct sockaddr_un new_un;
        new_un.sun_family = AF_UNIX;
        strncpy(new_un.sun_path, u->un_path, 108);
        memcpy(sockaddr, &new_un, size);
    }


    return sfd;
}

/**
 * @brief UNIX socket connect method
 * @param sock The socket to connect
 * @param sockaddr The socket address
 * @param addrlen The address length
 */
int unix_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    if (sock->type != SOCK_STREAM && sock->type != SOCK_SEQPACKET) return -EOPNOTSUPP;
    
    unix_sock_t *usock = USOCK(sock);

    if (addrlen != sizeof(struct sockaddr_un)) return -EINVAL;
    struct sockaddr_un *addr = (struct sockaddr_un*)sockaddr;

    // Canonicalize the path...
    char *p = vfs_canonicalizePath(current_cpu->current_process->wd_path, addr->sun_path);
    assert(p);

    // Get server
    sock_t *serv = hashmap_get(unix_path_map, p);
    if (!serv) return -ENOENT; // ???
    unix_sock_t *userv = USOCK(serv);

    if (!(serv->type == sock->type)) {
        return -EPROTOTYPE;
    }

    if (sock->type == SOCK_DGRAM) {
        // Don't actually need to do any work since this is a datagram
        usock->peer = userv;
        UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_CONNECTED);
        return 0;
    }

    // Make sure server is actually listening
    if (userv->state != UNIX_SOCK_STATE_LISTEN) return -ECONNREFUSED;

    // Create connection request
    unix_conn_req_t *r = kmalloc(sizeof(unix_conn_req_t));
    r->socket = sock;
    r->state = UNIX_CONN_WAITING;
    r->thr = current_cpu->current_thread;

    LOG(DEBUG, "UNIX connection...\n");

    // Insert us into queue
    queue_node_t *n = queue_node_create(r);
    mutex_acquire(userv->server.m);
    sleep_prepare();
    queue_push_node(userv->server.conn, n);
    sleep_wakeupQueue(userv->server.accepters, 1);
    fs_alert(userv->sock->node, VFS_EVENT_READ);
    mutex_release(userv->server.m);


    int attempts = 0;

    while (1) {
        attempts++;
        if (attempts >= 3) {
            LOG(WARN, "3 connection attempts expired on UNIX socket, assuming dead\n");
            atomic_store(&r->state, UNIX_CONN_DEAD);
            return -ETIMEDOUT;
        }
        
        int w = sleep_enter();
        if (w == WAKEUP_SIGNAL) { atomic_store(&r->state, UNIX_CONN_DEAD); return -EINTR; }

        if (r->state == UNIX_CONN_CONNECTED) {
            // Connected to the socket successfully
            break;
        }

        sleep_time(1, 0);
    }

    LOG(INFO, "Connection succeeded with %s\n", usock->peer->un_path);
    free(r);
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
    unix_sock_t *usock = USOCK(sock);

    if ((sock->type == SOCK_STREAM || sock->type == SOCK_SEQPACKET) && usock->state != UNIX_SOCK_STATE_CONNECTED) {
        // Peer not connected
        return -ENOTCONN;
    }
    
    if ((sock->type == SOCK_STREAM || sock->type == SOCK_SEQPACKET) && usock->state != UNIX_SOCK_STATE_CONNECTED && !usock->peer) {
        // No peer
        return -ECONNRESET;
    }

    if (usock->peer->state == UNIX_SOCK_STATE_CLOSED) {
        return 0;
    }

    // TODO: DGRAM

    // !!!: More than one iovec is not supported yet
    assert(msg->msg_iovlen == 1);

    ssize_t bytes_received = 0;
    if (sock->type == SOCK_DGRAM) {
        assert(0 && "SOCK_DGRAM not implemented yet");
    } else if (sock->type == SOCK_STREAM) {
        assert(0 && "SOCK_STREAM not implemented yet");
    } else if (sock->type == SOCK_SEQPACKET) {
        // SEQPACKET socket
        // Acquire Rx lock and try to get a packet
        for (;;) {
            spinlock_acquire(&usock->pkt.d->rx_lock);
            if (usock->pkt.d->rx_head) {
                // We have something in Rx head!
                
                // Remove it from Rx head
                unix_sock_packet_t *p = usock->pkt.d->rx_head;
                usock->pkt.d->rx_head = p->next;
                if (!usock->pkt.d->rx_head) usock->pkt.d->rx_tail = NULL;
                spinlock_release(&usock->pkt.d->rx_lock);

                // Copy it into the iovec buffer
                size_t sz = min(msg->msg_iov[0].iov_len, p->data_size);
                if (sz != p->data_size) {
                    LOG(WARN, "Losing some packet content (iovlen = %d data_size = %d)\n", msg->msg_iov[0].iov_len, p->data_size);
                    // TODO: Set MSG_TRUNC
                }

                memcpy(msg->msg_iov[0].iov_base, p->data, sz);
                bytes_received += sz;
                
                kfree(p);

                return bytes_received;
            }


            // Nope.. are we nonblocking?
            if (sock_nonblocking(sock)) {
                spinlock_release(&usock->pkt.d->rx_lock);
                return -EWOULDBLOCK;
            }

            // Check if our peer has closed
            if (usock->peer->state == UNIX_SOCK_STATE_CLOSED) {
                spinlock_release(&usock->pkt.d->rx_lock);
                unix_decrementAndFree(usock->peer);
                return -ECONNABORTED;
            }

            // Sleep in the queue
            LOG(DEBUG, "recvmsg, didn't get anything\n");
            sleep_inQueue(usock->pkt.d->rx_wait_queue);
            spinlock_release(&usock->pkt.d->rx_lock);
            int w = sleep_enter();
            if (w == WAKEUP_SIGNAL) return -EINTR;
        }
    } else {
        assert(0);
    }

    return bytes_received;
}


/**
 * @brief UNIX socket sendmsg method
 * @param sock The socket to send the message on
 * @param msg The message to send
 * @param flags Additional flags
 */
ssize_t unix_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    
    unix_sock_t *usock = USOCK(sock);

    // Ensure target
    if (sock->type != SOCK_DGRAM) {
        // Force connected state
        if (usock->state != UNIX_SOCK_STATE_CONNECTED) {
            return -ENOTCONN;
        }
        
        if (!usock->peer) {
            // We are in CONNECTED state but don't have a peer, peer was closed.
            return -ECONNABORTED;
        }
    }

    // Make sure we don't have more than one iovec
    assert(msg->msg_iovlen == 1);

    ssize_t total_sent = 0;
    if (sock->type == SOCK_DGRAM) {
        assert(!msg->msg_name && "msg->msg_name support not yet implemented");
        assert(0 && "SOCK_DGRAM not implemented yet");
    } else if (sock->type == SOCK_STREAM) {
        assert(0 && "SOCK_STREAM not implemented yet");
    } else if (sock->type == SOCK_SEQPACKET) {
        // TODO: tx_wait_queue usage, we need to not overwhelm the socket with information

        // Generate our request
        unix_sock_packet_t *p = kmalloc(sizeof(unix_sock_packet_t) + msg->msg_iov[0].iov_len);
        p->data_size = msg->msg_iov[0].iov_len;
        memcpy(p->data, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);
        p->next = NULL;

        spinlock_acquire(&usock->peer->pkt.d->rx_lock);
        
        // Add it to the tail
        if (usock->peer->pkt.d->rx_tail) {
            usock->peer->pkt.d->rx_tail->next = p;
            usock->peer->pkt.d->rx_tail = p;
        } else {
            usock->peer->pkt.d->rx_head = usock->peer->pkt.d->rx_tail = p;
        }
        
        // Wakeup a sleeper
        sleep_wakeupQueue(usock->peer->pkt.d->rx_wait_queue, 1);
        
        if (usock->peer->state == UNIX_SOCK_STATE_CLOSED) {
            LOG(ERR, "Peer socket is closed!\n");
            spinlock_release(&usock->peer->pkt.d->rx_lock);
            unix_decrementAndFree(usock->peer);
            return -ECONNRESET;
        }


        fs_alert(usock->peer->sock->node, VFS_EVENT_READ);
        spinlock_release(&usock->peer->pkt.d->rx_lock);


        total_sent += msg->msg_iov[0].iov_len;
    } else {
        assert(0);
    }

    return total_sent;
}

/**
 * @brief UNIX socket ready method
 * @param sock The socket
 * @param events Events to check for
 */
int unix_ready(sock_t *sock, int events) {
    unix_sock_t *usock = USOCK(sock);

    // Depending on what this socket is...
    if (usock->state == UNIX_SOCK_STATE_LISTEN) {
        // Listening
        return usock->server.conn->size ? VFS_EVENT_READ : 0;
    }

    if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
        if (sock->type == SOCK_DGRAM || sock->type == SOCK_SEQPACKET) {
            return usock->pkt.d->rx_head ? VFS_EVENT_READ : 0 | VFS_EVENT_WRITE;
        } else {
            return (circbuf_remaining_read(usock->stream.cb) ? VFS_EVENT_READ : 0) | (circbuf_remaining_write(usock->peer->stream.cb) ? VFS_EVENT_WRITE : 0);
        }
    }
    
    // The hell do you want to be ready for
    return 0;
}

/**
 * @brief UNIX close
 * @param sock The socket to close
 */
int unix_close(sock_t *sock) {
    unix_sock_t *usock = USOCK(sock);
    usock->state = UNIX_SOCK_STATE_CLOSED;

    unix_decrementAndFree(usock);

    return 0;
}

/**
 * @brief Create a UNIX socket
 */
sock_t *unix_socket(int type, int protocol) {
    if (type != SOCK_SEQPACKET && type != SOCK_STREAM && type != SOCK_DGRAM) return NULL;

    // Create UNIX socket data fields
    unix_sock_t *usock = kzalloc(sizeof(unix_sock_t));
    atomic_store(&usock->state, UNIX_SOCK_STATE_INIT);
    refcount_init(&usock->ref, 1);

    if (type == SOCK_SEQPACKET || type == SOCK_DGRAM) {
        // Initialize data
        usock->pkt.d = kzalloc(sizeof(unix_packet_data_t));
        usock->pkt.d->rx_wait_queue = sleep_createQueue("unix rx queue");
        usock->pkt.d->tx_wait_queue = sleep_createQueue("unix tx queue");
    } else if (type == SOCK_STREAM) {
        usock->stream.cb = circbuf_create("unix sock buffer", UNIX_SOCKET_BUFFER_SIZE);
    }


    sock_t *s = kzalloc(sizeof(sock_t));
    usock->sock = s;
    s->driver = (void*)usock;
    s->bind = unix_bind;
    s->accept = unix_accept;
    s->connect = unix_connect;
    s->listen = unix_listen;
    s->recvmsg = unix_recvmsg;
    s->sendmsg = unix_sendmsg;
    s->close = unix_close;
    s->ready = unix_ready;


    return s;
}

/**
 * @brief Initialize UNIX sockets
 */
static int unix_init() {
    unix_path_map = hashmap_create("unix path map", 10);
    return socket_register(AF_UNIX, unix_socket);
}

NET_INIT_ROUTINE(unix, INIT_FLAG_DEFAULT, unix_init, socket);