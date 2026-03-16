/**
 * @file hexahedron/drivers/net/socket_fs.c
 * @brief Contains the filesystem-related components of the socket system.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/net/socket.h>
#include <kernel/debug.h>
#include <kernel/fs/vfs_new.h>
#include <asm/ioctls.h>

/* File operations */
static int socketfs_open(vfs_file_t *file, unsigned long flags);
static ssize_t socketfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer);
static ssize_t socketfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer);
static int socketfs_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t socketfs_poll_events(vfs_file_t *file);
static int socketfs_ioctl(vfs_file_t *f, long request, void *argp);


static vfs_file_ops_t socket_file_ops = {
    .open           = socketfs_open,
    .close          = NULL,
    .read           = socketfs_read,
    .write          = socketfs_write,
    .ioctl          = socketfs_ioctl,
    .get_entries    = NULL,
    .poll           = socketfs_poll,
    .poll_events    = socketfs_poll_events,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
};


/* Inode operations */
static int socketfs_destroy(vfs_inode_t *inode);
static vfs_inode_ops_t socket_inode_ops = {
    .create     = NULL,
    .destroy    = socketfs_destroy,
    .getattr    = NULL,
    .link       = NULL,
    .lookup     = NULL,
    .mkdir      = NULL,
    .readlink   = NULL,
    .rmdir      = NULL,
    .setattr    = NULL,
    .symlink    = NULL,
    .truncate   = NULL,
    .unlink     = NULL,
    .rename     = NULL,
};

/**
 * @brief socketfs open
 */
static int socketfs_open(vfs_file_t *file, unsigned long flags) {
    file->priv = file->inode->priv;
    return 0;
}

/**
 * @brief socketfs read
 */
static ssize_t socketfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer) {
    sock_t *sock = (sock_t*)file->priv;
    if (sock->ops->recvmsg == NULL) return -ENODEV;

    struct iovec iov = { .iov_base = buffer, .iov_len = size };
    struct msghdr msg = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    return sock->ops->recvmsg(sock, &msg, 0);
}


/**
 * @brief socketfs write
 */
static ssize_t socketfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer) {
    sock_t *sock = (sock_t*)file->priv;
    if (sock->ops->sendmsg == NULL) return -ENODEV;

    struct iovec iov = { .iov_base = (char*)buffer, .iov_len = size };
    struct msghdr msg = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    return sock->ops->sendmsg(sock, &msg, 0);
}

/**
 * @brief socket ioctl
 */
static int socketfs_ioctl(vfs_file_t *f, long request, void *argp) {
    sock_t *sock = f->priv;
    switch (request) {
        case FIONBIO:
            SYSCALL_VALIDATE_PTR(argp);
            sock->flags |= (*(int*)argp ? SOCKET_FLAG_NONBLOCKING : 0);
            return 0;

        default:
            dprintf(WARN, "socket ioctl unsupported 0x%x\n", request);
            return -ENOSYS;
    }
}

/**
 * @brief socketfs poll
 */
static int socketfs_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events) {
    sock_t *sock = file->priv; 
    if (sock->ops->poll) return sock->ops->poll(sock, waiter, events);
    return poll_add(waiter, &sock->sock_event, events);
}

/**
 * @brief socketfs poll events
 */
static poll_events_t socketfs_poll_events(vfs_file_t *file) {
    sock_t *sock = file->priv;
    if (sock->ops->poll_events) return sock->ops->poll_events(sock);    

    poll_events_t revents = POLLOUT;
    if (sock->recv_queue->length) revents |= POLLIN;
    return revents;
}

/**
 * @brief Create a new socket inode
 * @param sock The socket to create the inode on
 */
vfs_inode_t *socketfs_create(sock_t *sock) {
    vfs_inode_t *i = vfs2_inode();
    i->priv = (void*)sock;
    i->attr.type = VFS_SOCKET;
    i->attr.uid = current_cpu->current_process->uid;
    i->attr.gid = current_cpu->current_process->gid;
    i->attr.ino = vfs_getNextInode();
    i->attr.mode = 0775;
    i->attr.nlink = 1;
    i->ops = &socket_inode_ops;
    i->f_ops = &socket_file_ops;
    i->mount = NULL; // this is totally gonna bite me later
    return i;
}

/**
 * @brief Destroy socket inode
 * @param inode The socket inode to destroy
 */
static int socketfs_destroy(vfs_inode_t *inode) {
    sock_t *sock = (sock_t*)inode->priv;
    
    if (sock->ops->close) sock->ops->close(sock);
    if (sock->recv_lock) spinlock_destroy(sock->recv_lock);
    if (sock->recv_queue) list_destroy(sock->recv_queue, true);
    if (sock->recv_wait_queue) kfree(sock->recv_wait_queue);
    socket_free(sock);
    return 0;
}