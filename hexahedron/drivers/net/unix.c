/**
 * @file hexahedron/drivers/net/unix.c
 * @brief UNIX sockets
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/net/unix.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/slab.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <string.h>

/* Caches */
slab_cache_t *unix_socket_cache = NULL;
slab_cache_t *unix_conn_cache = NULL;

/* Path map */
hashmap_t *unix_path_map = NULL;
mutex_t unix_path_lock = MUTEX_INITIALIZER;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVERS:NET:UNIX", __VA_ARGS__)

/* Get unix socket */
#define USOCK(sock) ((unix_socket_t*)sock->driver)

/* State (might be atomic in future) */
#define UNIX_STATE_CHANGE(usock, s) (usock)->state = s;
#define UNIX_GET_STATE(usock) ((usock)->state)

/* Refcount methods */
static void unix_free(unix_socket_t *usock);
#define UNIX_HOLD(u) (refcount_inc(&(u)->refs))
#define UNIX_RELEASE(u) if (refcount_dec(&(u)->refs) == 0) { unix_free(u); }

/* Sizes */
#define UNIX_DEFAULT_RB_SIZE        256 * 1024
#define UNIX_DEFAULT_QUEUE_SIZE     256

/* Update cred */
#define UNIX_UPDATE_CRED(usock)     (usock)->cred.pid = current_cpu->current_process->pid;\
                                    (usock)->cred.uid = current_cpu->current_process->uid;\
                                    (usock)->cred.gid = current_cpu->current_process->gid;

/* Ops */
static int unix_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static int unix_listen(sock_t *sock, int backlog);
static int unix_accept(sock_t *sock, struct sockaddr *sockaddr, socklen_t *addrlen);
static int unix_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static ssize_t unix_recvmsg(sock_t *sock, struct msghdr *msg, int flags);
static ssize_t unix_sendmsg(sock_t *sock, struct msghdr *msg, int flags);
static poll_events_t unix_poll_events(sock_t *sock);
static poll_events_t unix_poll_events_inner(unix_socket_t *usock);
static int unix_poll(sock_t *sock, poll_waiter_t *w, poll_events_t e);
static int unix_close(sock_t *sock);
static int unix_getsockname(sock_t *sock, struct sockaddr *addr, socklen_t *address_len);
static int unix_getpeername(sock_t *sock, struct sockaddr *addr, socklen_t *address_len);
static int unix_getsockopt(sock_t *sock, int level, int option_name, void *option_value, socklen_t *option_len);
static int unix_setsockopt(sock_t *sock, int level, int option_name, const void *option_value, socklen_t option_len);

static sock_ops_t unix_sock_ops = {
    .accept = unix_accept,
    .bind = unix_bind,
    .listen = unix_listen,
    .connect = unix_connect,
    .recvmsg = unix_recvmsg,
    .sendmsg = unix_sendmsg,
    .poll_events = unix_poll_events,
    .poll = unix_poll,
    .close = unix_close,
    .getpeername = unix_getpeername,
    .getsockname = unix_getsockname,
    .getsockopt = unix_getsockopt,
    .setsockopt = unix_setsockopt,
};


/**
 * @brief unix bind
 */
static int unix_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    unix_socket_t *usock = USOCK(sock);
    struct sockaddr_un *un = (struct sockaddr_un*)sockaddr;

    mutex_acquire(&usock->lock);

    if (usock->inode != NULL) {
        LOG(ERR, "Socket is already bound, failed to bind\n");
        mutex_release(&usock->lock);
        return -EINVAL;
    }

    // canonicalize the path
    char path[PATH_MAX];
    vfs_canonicalize(current_cpu->current_process->wd_path, un->sun_path, path);

    LOG(DEBUG, "UNIX socket binding to %s\n", path);

    // TODO: this inode must be created as a socket
    vfs_inode_t *i;
    int r = vfs_create(path, 0755, &i);
    if (r != 0) {
        mutex_release(&usock->lock);
        return r;
    }

    usock->inode = i;
    usock->path = strdup(path);

    mutex_release(&usock->lock);

    // add to map cache
    mutex_acquire(&unix_path_lock);
    hashmap_set(unix_path_map, usock->inode, usock);
    mutex_release(&unix_path_lock);

    return 0;
}

/**
 * @brief unix listen
 */
static int unix_listen(sock_t *sock, int backlog) {
    unix_socket_t *usock = USOCK(sock);
    mutex_acquire(&usock->lock);
    
    if (UNIX_GET_STATE(usock) != UNIX_SOCK_STATE_INIT) {
        LOG(ERR, "Socket attempted to listen in state %d\n", UNIX_GET_STATE(usock));
        mutex_release(&usock->lock);
        return -EINVAL;
    }

    // Prepare socket to listen
    QUEUE_RB_INIT(&usock->listen.backlog, backlog); // creates a ringbuffer, which is why it was delayed

    // Update cred
    UNIX_UPDATE_CRED(usock);

    // In listening state now
    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_LISTEN);

    mutex_release(&usock->lock);
    return 0;
}

/**
 * @brief unix accept
 */
static int unix_accept(sock_t *sock, struct sockaddr *sockaddr, socklen_t *addrlen) {
    unix_socket_t *usock = USOCK(sock);

    LOG(DEBUG, "trace: unix_accept on socket %p\n", usock);

    mutex_acquire(&usock->lock);
    if (UNIX_GET_STATE(usock) != UNIX_SOCK_STATE_LISTEN) {
        LOG(ERR, "UNIX socket cannot accept if not in listening state\n");
        mutex_release(&usock->lock);
        return -EINVAL;
    }

    if (usock->inode == NULL) {
        LOG(ERR, "UNIX socket cannot accept if not bound\n");
        mutex_release(&usock->lock);
        return -EINVAL;
    }

    // wait for a person to become available
    for (;;) {
        poll_events_t avail = unix_poll_events_inner(usock);
        if (avail & POLLIN) break;

        if (sock_nonblocking(sock)) {
            mutex_release(&usock->lock);
            return -EWOULDBLOCK;
        }

        // TODO avoid holding lock while allocating
        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &usock->event, POLLIN);
        mutex_release(&usock->lock);

        // TODO: timeout
        int r = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);

        if (r != 0) {
            return r;
        }

        mutex_acquire(&usock->lock);
    }

    // pop them
    unix_connection_req_t *req = NULL;
    queue_rb_pop(&usock->listen.backlog, (void**)&req);
    assert(req);

    // TODO: redo this once i add timeouts, right now client is forever sleeping    
    mutex_release(&usock->lock);

    // get needed fields and destroy req structure
    unix_socket_t *client = req->usock;
    thread_t *thr = req->thr;
    slab_free(unix_conn_cache, req);

    // create a new socket
    int sfd = socket_create(current_cpu->current_process, AF_UNIX, sock->type, sock->protocol);
    if (sfd < 0) {
        return sfd;
    }

    sock_t *new_sock = FD(sfd)->priv;
    unix_socket_t *new_usock = USOCK(new_sock);

    // hold the client and the new unix socket
    UNIX_HOLD(client);
    UNIX_HOLD(new_usock);
    client->peer = new_usock;
    new_usock->peer = client;

    // we need to initialize the new unix socket's data structures
    new_usock->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
    new_usock->path = strdup(usock->path);
    if (sock->type == SOCK_DGRAM || sock->type == SOCK_SEQPACKET) {
        QUEUE_RB_INIT(&new_usock->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
    }

    // advance both sockets to connected state
    UNIX_STATE_CHANGE(client, UNIX_SOCK_STATE_CONNECTED); // todo racey
    UNIX_STATE_CHANGE(new_usock, UNIX_SOCK_STATE_CONNECTED);

    // signal the new client
    poll_signal(&client->event, POLLOUT);

    // wakeup thread
    if (thr) sleep_wakeup(thr);

    // we need to fill sockaddr if they want if
    if (sockaddr) {
        // TODO: This is buggy
        assert(addrlen && (*addrlen) >= sizeof(struct sockaddr_un));

        struct sockaddr_un *un = (struct sockaddr_un *)sockaddr;
        un->sun_family = AF_UNIX;
        
        if (client->path) {
            strncpy(un->sun_path, client->path, 108);
        } else {
            un->sun_path[0] = 0;
        }
    }

    return sfd;
}



/**
 * @brief unix connect
 */
static int unix_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    if (sock->type != SOCK_STREAM && sock->type != SOCK_SEQPACKET) return -EOPNOTSUPP;
    if (addrlen < sizeof(struct sockaddr_un)) {
        LOG(ERR, "Tried to connect but passed an address length of %d\n", addrlen);
        return -EINVAL;
    }

    unix_socket_t *usock = USOCK(sock);
    mutex_acquire(&usock->lock);

    if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
        mutex_release(&usock->lock);
        return -EISCONN;
    }

    if (usock->state == UNIX_SOCK_STATE_CONNECTING) {
        mutex_release(&usock->lock);
        return -EAGAIN;
    }

    if (usock->state != UNIX_SOCK_STATE_INIT) {
        mutex_release(&usock->lock);
        return -EINVAL;
    }

    // Create the datastructures for the socket, if they dont exist
    if (usock->pkt.rb == NULL) {
        usock->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
        if (sock->type == SOCK_DGRAM || sock->type == SOCK_SEQPACKET) {
            QUEUE_RB_INIT(&usock->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
        }
    }

    // Update credentials
    UNIX_UPDATE_CRED(usock);

    struct sockaddr_un *addr = (struct sockaddr_un*)sockaddr;

    // Canonicalize the path...
    char p[PATH_MAX];
    vfs_canonicalize(current_cpu->current_process->wd_path, addr->sun_path, p);

    LOG(DEBUG, "UNIX socket connecting to %s\n", p);

    vfs_file_t *f;
    int r = vfs_open(addr->sun_path, O_RDWR, &f);
    if (r < 0) {
        mutex_release(&usock->lock);
        return r;
    }

    // resolve that socket
    mutex_acquire(&unix_path_lock);
    unix_socket_t *serv = hashmap_get(unix_path_map, f->inode);
    mutex_release(&unix_path_lock);

    vfs_close(f);

    if (!serv) {
        mutex_release(&usock->lock);
        return -ENOTSOCK;
    }

    // create a connection request
    unix_connection_req_t *req = slab_allocate(unix_conn_cache);
    req->thr = sock_nonblocking(sock) ? NULL : current_cpu->current_thread;
    req->usock = usock;

    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_CONNECTING);
    mutex_release(&usock->lock);

    // Now we can push into the server
    mutex_acquire(&serv->lock);

    // Wait until space in the backlog is available
    for (;;) {
        poll_events_t r = unix_poll_events_inner(serv);
        if (r & POLLOUT) {
            break;
        }

        if (sock_nonblocking(sock)) {
            mutex_release(&serv->lock);
            return -EAGAIN;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &serv->event, POLLOUT);
        mutex_release(&serv->lock);
        int ret = poll_wait(w, -1); // TODO timeout

        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            return ret;
        }

        mutex_acquire(&serv->lock);
    }

    // Push
    queue_rb_push(&serv->listen.backlog, req);
    poll_signal(&serv->event, POLLIN);

    if (!sock_nonblocking(sock)) {
        // we will need to be asleep for this
        // TODO: timeout
        sleep_prepare();
    }

    mutex_release(&serv->lock);
    
    if (sock_nonblocking(sock)) {
        return (usock->state == UNIX_SOCK_STATE_CONNECTED) ? 0 : -EWOULDBLOCK;
    } else {
        int w = sleep_enter();
        if (w != WAKEUP_ANOTHER_THREAD) {
            // !!! This might corrupt things.. maybe.
            LOG(WARN, "Connection failed.\n");
            
            if (w == WAKEUP_SIGNAL) return -EINTR;
            if (w == WAKEUP_TIME) return -ETIMEDOUT;

        }

        return 0;
    }
}

/**
 * @brief unix recvmsg
 */
static ssize_t unix_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    unix_socket_t *usock = USOCK(sock);
    if (msg->msg_iovlen == 0) return 0;

    mutex_acquire(&usock->lock);

    assert(msg->msg_iovlen == 1 && "recvmsg multiple iovecs not supported");

    if (usock->state != UNIX_SOCK_STATE_CONNECTED) {
        mutex_release(&usock->lock);
        return -ENOTCONN;
    }

    for (;;) {
        poll_events_t ev = unix_poll_events_inner(usock);
        if (ev & POLLIN) break;

        if (sock_nonblocking(sock)) {
            mutex_release(&usock->lock);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &usock->event, POLLIN);
        mutex_release(&usock->lock);
        int ret = poll_wait(w, -1); // TODO timeout

        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            return ret;
        }

        mutex_acquire(&usock->lock);
    }

    // We should have available content
    // TODO: receive control message

    ssize_t gotten = 0;
    size_t length = msg->msg_iov[0].iov_len;
    if (sock->type == SOCK_SEQPACKET || sock->type == SOCK_DGRAM) {
        size_t pkt_length;

        if (msg->msg_flags & MSG_PEEK) {
            assert(queue_rb_peek(&usock->pkt.queue, (void**)&pkt_length) == 0);
        } else {
            assert(queue_rb_pop(&usock->pkt.queue, (void**)&pkt_length) == 0);
        }

        if (length < pkt_length) {
            LOG(WARN, "Truncating SEQPACKET/DGRAM packet.\n");
            msg->msg_flags |= MSG_TRUNC;
        } else {
            length = pkt_length;
        }
    }

    ssize_t got = 0;
    if (msg->msg_flags & MSG_PEEK) {
        got = ringbuffer_peek(usock->pkt.rb, msg->msg_iov[0].iov_base, length);
    } else {
        got = ringbuffer_read(usock->pkt.rb, msg->msg_iov[0].iov_base, length);
        if (got) poll_signal(&usock->peer->event, POLLOUT);
    }

    mutex_release(&usock->lock);
    return got;
}


/**
 * @brief helper
 */
static void unix_lock(unix_socket_t *s1, unix_socket_t *s2) {
    if ((uintptr_t)s1 < (uintptr_t)s2) {
        mutex_acquire(&s1->lock);
        mutex_acquire(&s2->lock);
    } else {
        mutex_acquire(&s2->lock);
        mutex_acquire(&s1->lock);
    }
}

/**
 * @brief helper
 */
static void unix_unlock(unix_socket_t *s1, unix_socket_t *s2) {
    // order doesn't strictly matter but good practice
    if ((uintptr_t)s1 < (uintptr_t)s2) {
        mutex_release(&s2->lock);
        mutex_release(&s1->lock);
    } else {
        mutex_release(&s1->lock);
        mutex_release(&s2->lock);
    }
}

/**
 * @brief unix sendmsg
 */
static ssize_t unix_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    unix_socket_t *usock = USOCK(sock);
    if (msg->msg_iovlen == 0) return 0;

    mutex_acquire(&usock->lock);

    assert(msg->msg_iovlen == 1 && "recvmsg multiple iovecs not supported");

    if (usock->state != UNIX_SOCK_STATE_CONNECTED) {
        mutex_release(&usock->lock);
        return -ENOTCONN;
    }

    // !!! This is racey! Need to redo the locking pattern on this...
    unix_socket_t *tgt = usock->peer;
    mutex_release(&usock->lock);

    // Acquire both locks for super-safety
    unix_lock(usock, tgt);

    // Wait until space is available
    for (;;) {
        if (sock->type == SOCK_STREAM) {
            poll_events_t ev = unix_poll_events_inner(usock);
            if (ev & POLLOUT) break;
        } else {
            // as message boundaries need to be preserved this doesnt work
            // we need to have enough content and a non-empty queue
            if (ringbuffer_remaining_write(tgt->pkt.rb) >= msg->msg_iov[0].iov_len && queue_rb_space(&tgt->pkt.queue)) {
                break;
            }
        }

        if (sock_nonblocking(sock)) {
            unix_unlock(usock, tgt);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &usock->event, POLLOUT);
        unix_unlock(usock, tgt);
        int ret = poll_wait(w, -1); // TODO timeout

        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            return ret;
        }

        unix_lock(usock, tgt);
    }

    // !!! we dont need to hold the lock for usock here
    ssize_t written = ringbuffer_write(tgt->pkt.rb, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);

    if (sock->type == SOCK_SEQPACKET || sock->type == SOCK_DGRAM) {
        assert(written == (ssize_t)msg->msg_iov[0].iov_len);
        queue_rb_push(&tgt->pkt.queue, (void*)(uintptr_t)written);
    }

    if (written > 0) poll_signal(&tgt->event, POLLIN);
    unix_unlock(usock, tgt);

    return written;
}

/**
 * @brief unix poll events inner
 */
static poll_events_t unix_poll_events_inner(unix_socket_t *usock) {
    poll_events_t revents = 0;
    if (usock->state == UNIX_SOCK_STATE_LISTEN) {
        if (!queue_rb_empty(&usock->listen.backlog)) {
            revents |= POLLIN;
        }

        if (queue_rb_space(&usock->listen.backlog)) {
            revents |= POLLOUT;
        }
    } else if (usock->state == UNIX_SOCK_STATE_CONNECTING) {
        // do nothing, as this means that you cant do anything.
    } else if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
        if (ringbuffer_remaining_read(usock->pkt.rb)) {
            revents |= POLLIN;
        }

        if (usock->peer->state != UNIX_SOCK_STATE_CONNECTED) {
            revents |= POLLHUP;
        } else {
            if (ringbuffer_remaining_write(usock->peer->pkt.rb)) {
                revents |= POLLOUT;
            }
        }
    } else {
        // closed
        revents |= POLLHUP;
    }

    return revents;
}

/**
 * @brief unix poll events
 */
static poll_events_t unix_poll_events(sock_t *sock) {
    unix_socket_t *usock = USOCK(sock);

    mutex_acquire(&usock->lock);
    poll_events_t ret = unix_poll_events_inner(usock);
    mutex_release(&usock->lock);

    return ret;
}

/**
 * @brief unix poll
 */
static int unix_poll(sock_t *sock, poll_waiter_t *w, poll_events_t e) {
    unix_socket_t *usock = USOCK(sock);
    poll_add(w, &usock->event, e);
    return 0;
}

/**
 * @brief unix close
 */
static int unix_close(sock_t *sock) {
    unix_socket_t *usock = USOCK(sock);

    LOG(DEBUG, "unix_close\n");

    mutex_acquire(&usock->lock);
    if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
        UNIX_RELEASE(usock->peer);
        poll_signal(&usock->peer->event, POLLHUP);
    } else {
        assert(usock->state != UNIX_SOCK_STATE_CONNECTING && "close while connecting is stupid not impl'd");
    }
    
    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_CLOSED);
    mutex_release(&usock->lock);

    UNIX_RELEASE(usock);
    return 0;
}

/**
 * @brief unix getsockname
 */
static int unix_getsockname(sock_t *sock, struct sockaddr *addr, socklen_t *address_len) {
    return -ENOTSUP;
}

/**
 * @brief unix getpeername
 */
static int unix_getpeername(sock_t *sock, struct sockaddr *addr, socklen_t *address_len) {
    return -ENOTSUP;
}

/**
 * @brief unix getsockopt
 */
static int unix_getsockopt(sock_t *sock, int level, int option_name, void *option_value, socklen_t *option_len) {
    return -ENOPROTOOPT;
}

/**
 * @brief unix setsockopt
 */
static int unix_setsockopt(sock_t *sock, int level, int option_name, const void *option_value, socklen_t option_len) {
    return -ENOPROTOOPT;
}

/**
 * @brief Free a UNIX socket
 */
static void unix_free(unix_socket_t *usock) {
    if (usock->inode) {
        mutex_acquire(&unix_path_lock);
        hashmap_remove(unix_path_map, usock->inode);
        mutex_release(&unix_path_lock);

        inode_release(usock->inode);
    }

    if (usock->pkt.rb) {
        ringbuffer_destroy(usock->pkt.rb);
        
        if (usock->pkt.queue) {
            QUEUE_RB_DEINIT(&usock->pkt.queue);
        }
    }

    if (usock->listen.backlog) {
        
        QUEUE_RB_DEINIT(&usock->listen.backlog);
    }

    if (usock->path) {
        kfree(usock->path);
    }


}

/**
 * @brief Create UNIX socket
 */
static sock_t *unix_socket(int type, int protocol) {
    if (type != SOCK_SEQPACKET && type != SOCK_STREAM && type != SOCK_DGRAM) return NULL;

    // create the UNIX socket
    unix_socket_t *usock = slab_allocate(unix_socket_cache);
    memset(usock, 0, sizeof(unix_socket_t));
    MUTEX_INIT(&usock->lock);
    POLL_EVENT_INIT(&usock->event);
    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_INIT);
    refcount_init(&usock->refs, 1);

    // we can init the inner fields later, during bind() or connect()

    // create the actual socket
    sock_t *s = socket_allocate(AF_UNIX, type, protocol);
    usock->sock = s;
    s->driver = (void*)usock;
    s->ops = &unix_sock_ops;
    return s;
}

/**
 * @brief Initialize UNIX sockets
 */
static int unix_init() {
    unix_socket_cache = slab_createCache("unix socket cache", SLAB_CACHE_DEFAULT, sizeof(unix_socket_t), 0, NULL, NULL);
    unix_conn_cache = slab_createCache("unix conn cache", SLAB_CACHE_DEFAULT, sizeof(unix_connection_req_t), 0, NULL, NULL);
    unix_path_map = hashmap_create_int("unix path map", 10);

    socket_register(AF_UNIX, unix_socket);
    return 0;
}

NET_INIT_ROUTINE(unix, INIT_FLAG_DEFAULT, unix_init);
