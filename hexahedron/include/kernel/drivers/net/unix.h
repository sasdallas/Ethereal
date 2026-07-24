/**
 * @file hexahedron/include/kernel/drivers/net/unix.h
 * @brief UNIX socket subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_NET_UNIX_H
#define DRIVERS_NET_UNIX_H

/**** INCLUDES ****/
#include <kernel/drivers/net/socket.h>
#include <kernel/misc/waitqueue.h>
#include <kernel/misc/mutex.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/refcount.h>
#include <structs/queue_rb.h>
#include <stdint.h>
#include <sys/un.h>

/**** TYPES ****/

struct ucred {
	pid_t pid;
	uid_t uid;
	gid_t gid;
};

typedef enum {
    UNIX_SOCK_STATE_INIT,
    UNIX_SOCK_STATE_LISTEN,
    UNIX_SOCK_STATE_CONNECTING,
    UNIX_SOCK_STATE_CONNECTED,
    UNIX_SOCK_STATE_CLOSED
} unix_sock_state_t;

struct unix_socket;

typedef struct unix_connection_req {
    struct unix_socket *usock;
    struct thread *thr;
} unix_connection_req_t; 

typedef struct unix_socket {
    struct unix_socket *peer;   // socket peer, a ref is held on them
    char *path;                 // bound path
    mutex_t lock;               // held for accesses
    sock_t *sock;
    unix_sock_state_t state;
    vfs_inode_t *inode;         // this is the backing inode
    poll_event_t event;
    struct ucred cred;          // credentials for SO_PEERCRED

    struct {
        ringbuffer_t *rb;
        queue_rb_t queue; // for SEQPACKET/DGRAM
    } pkt;

    struct {
        queue_rb_t backlog;
    } listen;


    refcount_t refs;
} unix_socket_t;

#endif
