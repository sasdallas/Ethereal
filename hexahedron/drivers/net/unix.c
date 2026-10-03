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
#define UNIX_DEFAULT_QUEUE_SIZE     512 // Double rb because each pkt is length followed by unix socket pointer

/* Update cred */
#define UNIX_UPDATE_CRED(usock)     (usock)->cred.pid = current_cpu->current_process->pid;\
                                    (usock)->cred.uid = current_cpu->current_process->cred.uid;\
                                    (usock)->cred.gid = current_cpu->current_process->cred.gid;

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
    process_t *proc = current_cpu->current_process;
    int r = vfs_create(path, 0777 & ~proc->umask, &i);
    if (r != 0) {
        mutex_release(&usock->lock);
        return r;
    }

    // When creating a socket it must be owned by the EUID/EGID and not UID/GID
    r = vfs_chown(i, proc->cred.euid, proc->cred.egid);
    if (r != 0) {
        vfs_unlinkat(NULL, path);
        inode_release(i);
        mutex_release(&usock->lock);
        return r;
    }

    usock->inode = i;
    usock->path = strdup(path);
    memcpy(&usock->bound, sockaddr, min(addrlen, sizeof(struct sockaddr_un)));

    // Create socket datastructures if they dont exist
    if (usock->pkt.rb == NULL) {
        usock->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
        QUEUE_RB_INIT(&usock->pkt.control, UNIX_DEFAULT_QUEUE_SIZE);
        if (sock->type == SOCK_DGRAM || sock->type == SOCK_SEQPACKET) {
            QUEUE_RB_INIT(&usock->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
        }
    }

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
    QUEUE_RB_INIT(&new_usock->pkt.control, UNIX_DEFAULT_QUEUE_SIZE);
    new_usock->path = strdup(usock->path);
    memcpy(&new_usock->bound, &usock->bound, sizeof(struct sockaddr_un));
    if (sock->type == SOCK_DGRAM || sock->type == SOCK_SEQPACKET) {
        QUEUE_RB_INIT(&new_usock->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
    }

    UNIX_UPDATE_CRED(new_usock);

    // advance both sockets to connected state
    UNIX_STATE_CHANGE(client, UNIX_SOCK_STATE_CONNECTED); // todo racey
    UNIX_STATE_CHANGE(new_usock, UNIX_SOCK_STATE_CONNECTED);

    // signal the new client
    poll_signal(&client->event, POLLOUT);

    // wakeup thread
    if (thr) sleep_wakeup(thr);

    // we need to fill sockaddr if they want if
    if (sockaddr && addrlen) {
        // TODO: This is buggy
        assert((*addrlen) >= sizeof(sa_family_t));

        struct sockaddr_un *un = (struct sockaddr_un *)sockaddr;
        un->sun_family = AF_UNIX;
        
        size_t rem = sizeof(struct sockaddr_un) - sizeof(sa_family_t);
        if (rem) {
            if (client->path) {
                strncpy(un->sun_path, client->path, rem);
            } else {
                un->sun_path[0] = 0;
            }
        }
    }

    return sfd;
}



/**
 * @brief unix connect
 */
static int unix_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    if (addrlen < sizeof(sa_family_t)+1) {
        LOG(ERR, "Tried to connect but passed an address length of %d\n", addrlen);
        return -EINVAL;
    }

    unix_socket_t *usock = USOCK(sock);
    mutex_acquire(&usock->lock);

    if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
        if (sock->type == SOCK_DGRAM) {
            UNIX_RELEASE(usock->peer);
            usock->peer = NULL;
        } else {
            mutex_release(&usock->lock);
            return -EISCONN;
        }
    }

    if (usock->state == UNIX_SOCK_STATE_CONNECTING) {
        mutex_release(&usock->lock);
        return -EALREADY;
    }

    if (usock->state != UNIX_SOCK_STATE_INIT) {
        if (sock->type != SOCK_DGRAM) {
            mutex_release(&usock->lock);
            return -EINVAL;
        }
    }

    // Create the datastructures for the socket, if they dont exist
    if (usock->pkt.rb == NULL) {
        usock->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
        QUEUE_RB_INIT(&usock->pkt.control, UNIX_DEFAULT_QUEUE_SIZE);
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

    // DGRAM sockets dont need to be acknowledged
    if (sock->type == SOCK_DGRAM) {
        UNIX_HOLD(serv);
        memcpy(&usock->dfl, sockaddr, min(addrlen, sizeof(struct sockaddr_un)));
        
        usock->peer = serv;
        UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_CONNECTED);
        
        mutex_release(&usock->lock);
        return 0;
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
        return (usock->state == UNIX_SOCK_STATE_CONNECTED) ? 0 : -EINPROGRESS;
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
 * @brief Release a UNIX control message
 */
static void unix_freeControl(unix_control_message_t *control) {
    for (size_t i = 0; i < control->file_count; i++) {
        FD_FINISH(control->files[i]);
    }

    kfree(control);
}

/**
 * @brief Create a control message
 */
static int unix_createControl(struct msghdr *msg, unix_control_message_t **output) {
    if (msg->msg_controllen < sizeof(struct cmsghdr)) {
        LOG(WARN, "msg_controllen = %d\n", msg->msg_controllen);
        return -EINVAL;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        LOG(WARN, "Unexpected cmsg_level %d cmsg_type %d\n", cmsg->cmsg_level, cmsg->cmsg_type);
        return -EINVAL;
    }

    if (cmsg->cmsg_len < CMSG_LEN(sizeof(int)) || cmsg->cmsg_len > msg->msg_controllen) {
        LOG(ERR, "Invalid cmsg_len %d\n", cmsg->cmsg_len);
        return -EINVAL;
    }

    size_t data_size = cmsg->cmsg_len - CMSG_LEN(0);
    if (data_size % sizeof(int)) {
        LOG(ERR, "Invalid data_size %d\n", data_size);
        return -EINVAL;
    }

    size_t count = data_size / sizeof(int);
    if (count > PROCESS_MAX_FDS) {
        LOG(ERR, "Too many file descriptors in SCM_RIGHTS (%d)\n", count);
        return -EMSGSIZE;
    }

    // Create the control message
    unix_control_message_t *control = kmalloc(sizeof(unix_control_message_t) + count * sizeof(vfs_file_t*));
    control->position = 0;
    control->length = 0;
    control->file_count = count;

    // Gather fds into message
    int *fds = (int*)CMSG_DATA(cmsg);
    for (size_t i = 0; i < count; i++) {
        int r = fd_get(fds[i], &control->files[i]);
        if (r != 0) {
            control->file_count = i;
            unix_freeControl(control);
            return r;
        }
    }

    *output = control;
    return 0;
}

/**
 * @brief Process a control message
 */
static int unix_processControl(struct msghdr *msg, int flags, size_t capacity, unix_control_message_t *control) {
    size_t count = control->file_count;
    while (count && CMSG_SPACE(count * sizeof(int)) > capacity) count--;

    if (count != control->file_count) {
        msg->msg_flags |= MSG_CTRUNC;
    }
    
    if (count == 0 || msg->msg_control == NULL) {
        return 0;
    }

    struct cmsghdr *cmsg = msg->msg_control;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(count * sizeof(int));

    int *fds = (int*)CMSG_DATA(cmsg);
    for (size_t i = 0; i < count; i++) {
        FD_HOLD(control->files[i]);
        int r = fd_add(control->files[i], &fds[i]);
        if (r != 0) {
            FD_FINISH(control->files[i]);
            for (size_t j = 0; j < i; j++) fd_remove(fds[j]);
            return r;
        }

        if (flags & MSG_CMSG_CLOEXEC) {
            fd_setCloseExecute(fds[i], true);
        }
    }

    msg->msg_controllen = CMSG_SPACE(count * sizeof(int));
    return 0;
}

/**
 * @brief Receive a control message
 */
static int unix_receiveControl(unix_socket_t *usock, struct msghdr *msg, int flags, size_t capacity, size_t *length) {
    if (queue_rb_empty(&usock->pkt.control)) return 0;

    unix_control_message_t *control = NULL;
    assert(queue_rb_peek(&usock->pkt.control, (void**)&control) == 0);

    if (control->position > usock->pkt.bytes_read) {
        *length = min(*length, control->position - usock->pkt.bytes_read);
    } else if (*length) {
        int r = unix_processControl(msg, flags, capacity, control);
        if (r != 0) return r;

        *length = min(*length, control->length);

        if ((flags & MSG_PEEK) == 0) {
            assert(queue_rb_pop(&usock->pkt.control, (void**)&control) == 0);
            unix_freeControl(control);
        }
    }

    return 0;
}

/**
 * @brief Send control message
 */
static void unix_sendControl(unix_socket_t *tgt, unix_control_message_t *control, size_t written) {
    control->position = tgt->pkt.bytes_written;
    control->length = written;
    queue_rb_push(&tgt->pkt.control, control);
}

/**
 * @brief unix recvmsg
 */
static ssize_t unix_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    unix_socket_t *usock = USOCK(sock);
    if (msg->msg_iovlen == 0) return 0;

    size_t control_capacity = msg->msg_controllen;
    msg->msg_controllen = 0;

    // No socket types support this yet unfortunately
    if (msg->msg_name != NULL) {
        LOG(ERR, "msg_name is not supported in recvmsg!\n");
    }

    mutex_acquire(&usock->lock);

    if (usock->state != UNIX_SOCK_STATE_CONNECTED && sock->type != SOCK_DGRAM) {
        mutex_release(&usock->lock);
        return -ENOTCONN;
    }

    for (;;) {
        poll_events_t ev = unix_poll_events_inner(usock);
        if (ev & POLLIN) break;

        if ((ev & POLLHUP) && sock->type != SOCK_DGRAM) {
            mutex_release(&usock->lock);
            return 0;
        }

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

    // temporary iov hack
    size_t length = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) length += msg->msg_iov[i].iov_len;
    
    // Receive control message if possible
    int r = unix_receiveControl(usock, msg, flags, control_capacity, &length);
    if (r != 0) {
        mutex_release(&usock->lock);
        return r;
    }

    size_t pkt_length = 0;
    if (sock->type == SOCK_SEQPACKET || sock->type == SOCK_DGRAM) {
        if (flags & MSG_PEEK) {
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
    if (flags & MSG_PEEK) {
        // ringbuffer_peek can only peek from the start of the buffer, so only the first vector is filled
        got = ringbuffer_peek(usock->pkt.rb, msg->msg_iov[0].iov_base, min(length, msg->msg_iov[0].iov_len));
    } else {
        for (int i = 0; i < msg->msg_iovlen && (size_t)got < length; i++) {
            size_t chunk = min(msg->msg_iov[i].iov_len, length - got);
            if (!chunk) continue;

            ssize_t r = ringbuffer_read(usock->pkt.rb, msg->msg_iov[i].iov_base, chunk);
            if (r <= 0) break;
            got += r;
            if ((size_t)r < chunk) break;
        }

        if (got >= 0 && (sock->type == SOCK_SEQPACKET || sock->type == SOCK_DGRAM)) {
            if (pkt_length > (size_t)got) {
                ringbuffer_discard(usock->pkt.rb, pkt_length - got);
            }

            usock->pkt.bytes_read += pkt_length;
        } else if (got > 0) {
            usock->pkt.bytes_read += got;
        }

        if (got && (usock->state == UNIX_SOCK_STATE_CONNECTED)) {
            poll_signal(&usock->peer->event, POLLOUT);
        }
    }

    mutex_release(&usock->lock);
    return got;
}


/**
 * @brief helper
 */
static void unix_lock(unix_socket_t *s1, unix_socket_t *s2) {
    MUTEX_LOCK_BOTH(&s1->lock, &s2->lock);
}

/**
 * @brief helper
 */
static void unix_unlock(unix_socket_t *s1, unix_socket_t *s2) {
    MUTEX_RELEASE_BOTH(&s1->lock, &s2->lock);
}

/**
 * @brief resolve target for sendmsg
 */
static int unix_resolve(unix_socket_t *usock, struct msghdr *msg, unix_socket_t **tgt) {
    if (usock->sock->type != SOCK_DGRAM) {
        goto _peer;
    }

    if (msg->msg_name) {
        if (msg->msg_namelen < sizeof(struct sockaddr_un)) {
            LOG(ERR, "Invalid namelen %d\n", msg->msg_namelen);
            return -EINVAL;
        }

        struct sockaddr_un *un = msg->msg_name;
        
        vfs_file_t *f;
        int r = vfs_open(un->sun_path, O_WRONLY, &f);
        if (r != 0) {
            return r;
        }

        mutex_acquire(&unix_path_lock);
        unix_socket_t *serv = hashmap_get(unix_path_map, f->inode);
        if (serv) UNIX_HOLD(serv);
        mutex_release(&unix_path_lock);
        
        vfs_close(f);

        if (serv) {
            *tgt = serv;
            return 0;
        }
    }

_peer:
    if (usock->state != UNIX_SOCK_STATE_CONNECTED) {
        return -ENOTCONN;
    }

    *tgt = usock->peer;
    UNIX_HOLD(usock->peer);
    return 0;
}

/**
 * @brief unix sendmsg
 */
static ssize_t unix_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    unix_socket_t *usock = USOCK(sock);
    if (msg->msg_iovlen == 0) return 0;

    size_t length = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) length += msg->msg_iov[i].iov_len;

    // Create the control message if needed
    unix_control_message_t *control = NULL;
    if (msg->msg_control != NULL && msg->msg_controllen != 0) {
        if (length == 0) return -EINVAL;
        int r = unix_createControl(msg, &control);
        if (r != 0) return r;
    }

    // Other socket don't suport this yet
    if (sock->type != SOCK_DGRAM) {
        assert(msg->msg_name == NULL);
    }

    mutex_acquire(&usock->lock);

    // !!! This is racey! Need to redo the locking pattern on this...
    unix_socket_t *tgt;
    int r = unix_resolve(usock, msg, &tgt);
    mutex_release(&usock->lock);

    if (r != 0) {
        if (control) unix_freeControl(control);
        return r;
    }

    // Acquire both locks for super-safety
    unix_lock(usock, tgt);

    // Wait until space is available
    for (;;) {
        if (sock->type != SOCK_DGRAM && tgt->state != UNIX_SOCK_STATE_CONNECTED) {
            unix_unlock(usock, tgt);
            UNIX_RELEASE(tgt);
            if (control) unix_freeControl(control);

            if (!(flags & MSG_NOSIGNAL)) {
                signal_send(current_cpu->current_process, SIGPIPE);
            }
            
            return -EPIPE;
        }

        if (sock->type == SOCK_STREAM) {
            poll_events_t ev = unix_poll_events_inner(usock);
            if ((ev & POLLOUT) && (!control || queue_rb_space(&tgt->pkt.control))) break;
        } else {
            // as message boundaries need to be preserved this doesnt work
            // we need to have enough content and a non-empty queue
            if (ringbuffer_remaining_write(tgt->pkt.rb) >= length && queue_rb_space(&tgt->pkt.queue) && (!control || queue_rb_space(&tgt->pkt.control))) {
                break;
            }
        }

        if (sock_nonblocking(sock)) {
            unix_unlock(usock, tgt);
            UNIX_RELEASE(tgt);
            if (control) unix_freeControl(control);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &usock->event, POLLOUT);
        unix_unlock(usock, tgt);
        int ret = poll_wait(w, -1); // TODO timeout

        poll_exit(w);
        poll_destroyWaiter(w);

        if (ret != 0) {
            UNIX_RELEASE(tgt);
            if (control) unix_freeControl(control);
            return ret;
        }

        unix_lock(usock, tgt);
    }

    // !!! we dont need to hold the lock for usock here
    ssize_t written = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        if (!msg->msg_iov[i].iov_len) continue;

        ssize_t r = ringbuffer_write(tgt->pkt.rb, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
        if (r <= 0) break;
        written += r;
        if ((size_t)r < msg->msg_iov[i].iov_len) break;
    }

    if (control) {
        if (written > 0) {
            unix_sendControl(tgt, control, written);
            control = NULL; // stop it from being freed
        }
    }

    tgt->pkt.bytes_written += written;

    if (sock->type == SOCK_SEQPACKET || sock->type == SOCK_DGRAM) {
        assert(written == (ssize_t)length);
        queue_rb_push(&tgt->pkt.queue, (void*)(uintptr_t)written);
    }

    if (written > 0) {
        poll_signal(&tgt->event, POLLIN);
    }

    unix_unlock(usock, tgt);

    // tgt gets a reference regardless
    UNIX_RELEASE(tgt);
    if (control) unix_freeControl(control);
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
    } else if (usock->pkt.rb != NULL) {
        if (ringbuffer_remaining_read(usock->pkt.rb)) {
            revents |= POLLIN;
        }

        if (usock->state == UNIX_SOCK_STATE_CONNECTED) {
            if (usock->peer->state != UNIX_SOCK_STATE_CONNECTED) {
                revents |= POLLHUP;
            } else {
                // Packet sockets need a queue slot as well as byte space.
                if (ringbuffer_remaining_write(usock->peer->pkt.rb) &&
                    (usock->sock->type == SOCK_STREAM || queue_rb_space(&usock->peer->pkt.queue))) {
                    revents |= POLLOUT;
                }
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

    unix_socket_t *peer = usock->state == UNIX_SOCK_STATE_CONNECTED ? usock->peer : NULL;
    if (peer) unix_lock(usock, peer);
    else mutex_acquire(&usock->lock);

    assert(usock->state != UNIX_SOCK_STATE_CONNECTING && "close while connecting is stupid not impl'd");
    UNIX_STATE_CHANGE(usock, UNIX_SOCK_STATE_CLOSED);

    if (peer) {
        poll_signal(&peer->event, POLLHUP);
        unix_unlock(usock, peer);
        UNIX_RELEASE(peer);
    } else {
        mutex_release(&usock->lock);
    }

    UNIX_RELEASE(usock);
    return 0;
}

/**
 * @brief unix getsockname
 */
static int unix_getsockname(sock_t *sock, struct sockaddr *addr, socklen_t *address_len) {
    unix_socket_t *usock = USOCK(sock);

    // Create a copy of the socket's address
    mutex_acquire(&usock->lock);
    struct sockaddr_un out = { 0 };
    out.sun_family = AF_UNIX;

    socklen_t len = sizeof(sa_family_t);
    if (usock->path) {
        strncpy(out.sun_path, usock->bound.sun_path, sizeof(out.sun_path) - 1);
        len = __builtin_offsetof(struct sockaddr_un, sun_path) + strlen(out.sun_path) + 1;
    }
    mutex_release(&usock->lock);

    // Copy it out
    size_t to_copy = min(len, *address_len);
    memcpy(addr, &out, to_copy);
    *address_len = len;
    return 0;
}

/**
 * @brief unix getpeername
 */
static int unix_getpeername(sock_t *sock, struct sockaddr *addr, socklen_t *address_len) {
    unix_socket_t *usock = USOCK(sock);

    // Get the peer
    mutex_acquire(&usock->lock);
    if (UNIX_GET_STATE(usock) != UNIX_SOCK_STATE_CONNECTED || !usock->peer) {
        mutex_release(&usock->lock);
        return -ENOTCONN;
    }

    unix_socket_t *peer = usock->peer;
    UNIX_HOLD(peer);
    mutex_release(&usock->lock);

    // Create a copy of the peer's bound socket
    mutex_acquire(&peer->lock);
    struct sockaddr_un out = { 0 };
    out.sun_family = AF_UNIX;
    socklen_t len = sizeof(sa_family_t);
    if (peer->path) {
        strncpy(out.sun_path, peer->bound.sun_path, sizeof(out.sun_path) - 1);
        len = __builtin_offsetof(struct sockaddr_un, sun_path) + strlen(out.sun_path) + 1;
    }
    mutex_release(&peer->lock); 
    UNIX_RELEASE(peer);

    // Copy it out
    size_t to_copy = min(len, *address_len);
    memcpy(addr, &out, to_copy);
    *address_len = len;
    return 0;
}

/**
 * @brief unix getsockopt
 */
static int unix_getsockopt(sock_t *sock, int level, int option_name, void *option_value, socklen_t *option_len) {
    unix_socket_t *usock = USOCK(sock);

    if (level != SOL_SOCKET) {
        return -ENOPROTOOPT;
    }

    switch (option_name) {
        case SO_PEERCRED: {
            if (!option_value || !option_len) {
                // dbus tried this at one point
                LOG(ERR, "SO_PEERCRED with stupid parameters\n");
                return -EINVAL;
            }

            size_t to_copy = min(*option_len, sizeof(struct ucred));

            // !!! racey
            mutex_acquire(&usock->lock);
            if (usock->state != UNIX_SOCK_STATE_CONNECTED || !usock->peer) {
                mutex_release(&usock->lock);
                return -ENOTCONN;
            }

            unix_socket_t *peer = usock->peer;
            UNIX_HOLD(peer);
            mutex_release(&usock->lock);

            mutex_acquire(&peer->lock);
            memcpy(option_value, &peer->cred, to_copy);
            mutex_release(&peer->lock);

            UNIX_RELEASE(peer);

            *option_len = sizeof(struct ucred);
            return 0;
        }
    }

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

        if (usock->pkt.control) {
            unix_control_message_t *control;
            while (queue_rb_pop(&usock->pkt.control, (void**)&control) == 0) {
                unix_freeControl(control);
            }
            QUEUE_RB_DEINIT(&usock->pkt.control);
        }
        
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
 * @brief unix socketpair
 */
static int unix_socketpair(int type, int protocol, sock_t* output[2]) {
    sock_t *a = unix_socket(type, protocol);
    sock_t *b = unix_socket(type, protocol);
    
    unix_socket_t *au = USOCK(a);
    unix_socket_t *bu = USOCK(b);

    au->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
    bu->pkt.rb = ringbuffer_create(UNIX_DEFAULT_RB_SIZE);
    QUEUE_RB_INIT(&au->pkt.control, UNIX_DEFAULT_QUEUE_SIZE);
    QUEUE_RB_INIT(&bu->pkt.control, UNIX_DEFAULT_QUEUE_SIZE);
    if (type == SOCK_DGRAM || type == SOCK_SEQPACKET) {
        QUEUE_RB_INIT(&au->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
        QUEUE_RB_INIT(&bu->pkt.queue, UNIX_DEFAULT_QUEUE_SIZE);
    }

    au->state = UNIX_SOCK_STATE_CONNECTED;
    bu->state = UNIX_SOCK_STATE_CONNECTED;

    au->peer = bu;
    bu->peer = au;

    UNIX_HOLD(au);
    UNIX_HOLD(bu);

    output[0] = a;
    output[1] = b;
    return 0;
}

/**
 * @brief Initialize UNIX sockets
 */
static int unix_init() {
    unix_socket_cache = slab_createCache("unix socket cache", SLAB_CACHE_DEFAULT, sizeof(unix_socket_t), 0, NULL, NULL);
    unix_conn_cache = slab_createCache("unix conn cache", SLAB_CACHE_DEFAULT, sizeof(unix_connection_req_t), 0, NULL, NULL);
    unix_path_map = hashmap_create_int("unix path map", 10);

    socket_register(AF_UNIX, unix_socket);
    socket_registerPair(AF_UNIX, unix_socketpair);
    return 0;
}

NET_INIT_ROUTINE(unix, INIT_FLAG_DEFAULT, unix_init);
