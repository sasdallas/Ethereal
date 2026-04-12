/**
 * @file hexahedron/fs/pipe.c
 * @brief Ethereal pipe implementation
 * 
 * This is just a simple UNIX pipe implementation.
 * 
 * @todo This is a very weak pipe implementation and should be replaced soon
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/pipe.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <kernel/task/process.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* Fipipele operations */
static int pipe_open(vfs_file_t *file, unsigned long flags);
static int pipe_close(vfs_file_t *file);
static ssize_t pipe_read(vfs_file_t *file, loff_t off, size_t size, char *buffer);
static ssize_t pipe_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer);
static poll_events_t pipe_poll_events(vfs_file_t *file);
static int pipe_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events);

static vfs_file_ops_t pipe_file_ops = {
    .open           = pipe_open,
    .close          = pipe_close,
    .read           = pipe_read,
    .write          = pipe_write,
    .ioctl          = NULL,
    .get_entries    = NULL,
    .poll           = pipe_poll,
    .poll_events    = pipe_poll_events,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
};


/* Inode operations */
static int pipe_destroy(vfs_inode_t *inode);
static vfs_inode_ops_t pipe_inode_ops = {
    .create     = NULL,
    .destroy    = pipe_destroy,
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

/* pipe cache */
slab_cache_t *pipe_cache = NULL;

/* is write */
#define IS_WRITE(p) ((p)->inode->attr.rdev == 1)

/**
 * @brief Pipe cache object initializer
 */
static int pipe_initializer(slab_cache_t *cache, void *obj) {
    dprintf(DEBUG, "pipe_initializer\n");
    fs_pipe_t *p = obj;
    POLL_EVENT_INIT(&p->read_event);
    POLL_EVENT_INIT(&p->write_event);
    p->dead = 0;
    p->buf = circbuf_create("pipe buffer", 1000);
    return 0;
}

/**
 * @brief Pipe cache object deinitializer
 */
static int pipe_deinitializer(slab_cache_t *cache, void *object) {
    fs_pipe_t *p = object;
    circbuf_destroy(p->buf);
    return 0;
}

/**
 * @brief pipe open
 */
static int pipe_open(vfs_file_t *file, unsigned long flags) {
    // TODO: blocking
    file->priv = file->inode->priv;
    return 0;
}

/**
 * @brief pipe close
 */
static int pipe_close(vfs_file_t *file) {
    // do the actual destruction in pipe_destroy
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;
    poll_signal(&pipe->read_event, POLLHUP);
    poll_signal(&pipe->write_event, POLLERR);
    circbuf_stop(pipe->buf);
    return 0;
}

/**
 * @brief pipe read
 */
static ssize_t pipe_read(vfs_file_t *file, loff_t off, size_t size, char *buffer) {
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;

    if (file->flags & O_NONBLOCK) {
        if (circbuf_remaining_read(pipe->buf) == 0) {
            return -EAGAIN;
        }
    }

    // Start reading from the circular buffer
    // circbuf_stop ensures that we will EOF on no more content
    ssize_t r = circbuf_read(pipe->buf, size, (uint8_t*)buffer);
    if (r >= 0) poll_signal(&pipe->write_event, POLLOUT);
    return r;
}

/**
 * @brief Write to a pipe
 * @param node The pipe node to read from
 * @param off The offset of the read
 * @param size The size of the read
 * @param buffer The buffer to read to
 */
static ssize_t pipe_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer) {
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;
    if (pipe->dead) {
        dprintf(ERR, "Pipe is dead need to send SIGPIPE\n");
        return -EPIPE; // TODO: track readers and send SIGPIPE
    }

    // TODO: this needs to O_NONBLOCK? system doesn't work without it will fix later
    if (!circbuf_remaining_write(pipe->buf)) return 0;
    ssize_t r = circbuf_write(pipe->buf, size, (uint8_t*)buffer);
    if (r >= 0) poll_signal(&pipe->read_event, POLLIN);
    return r;
}

/**
 * @brief pipe poll events
 */
static poll_events_t pipe_poll_events(vfs_file_t *file) {
    fs_pipe_t *p = file->priv;
    
    poll_events_t events = 0;
    if (p->dead) {
        if (IS_WRITE(file)) events |= POLLERR;
        else events |= POLLHUP;
        return events;
    }

    if (IS_WRITE(file)) {
        events |= (circbuf_remaining_write(p->buf) ? POLLOUT : 0);
    } else {
        events |= (circbuf_remaining_read(p->buf) ? POLLIN : 0);
    }

    return events;
}

/**
 * @brief pipe poll
 */
static int pipe_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events) {
    fs_pipe_t *p = file->priv;

    if (IS_WRITE(file)) {
        poll_add(waiter, &p->write_event, events);
    } else {
        poll_add(waiter, &p->read_event, events);
    }

    return 0;
}

/**
 * @brief pipe destroy
 */
static int pipe_destroy(vfs_inode_t *inode) {
    fs_pipe_t *pipe = inode->priv;

    // this trick was sourced from stanix, thanks tayoky
    if (atomic_exchange(&pipe->dead, 1)) {
        slab_free(pipe_cache, pipe);
    }

    return 0;
}

/**
 * @brief Create a new pipe set for a process
 * @param fildes The file descriptor array to fill with pipes
 * @returns Error code
 */
int pipe_create(int fildes[2]) {
    fs_pipe_t *pipe = slab_allocate(pipe_cache);
    if (!pipe) return -ENOMEM;

    // TODO: this is stupid but "reliable", i guess. will fix in pipe rev 2
    vfs_inode_t *read_node = vfs2_inode();
    read_node->attr.type = VFS_PIPE;
    read_node->attr.rdev = 0;
    read_node->attr.ino = vfs_getNextInode();
    read_node->ops = &pipe_inode_ops;
    read_node->f_ops = &pipe_file_ops;
    read_node->priv = pipe;

    vfs_inode_t *write_node = vfs2_inode();
    write_node->attr.type = VFS_PIPE;
    write_node->attr.rdev = 1; // what i want to do is use the file's opening flags but we dont get those do we
    write_node->attr.ino = vfs_getNextInode();
    write_node->ops = &pipe_inode_ops;
    write_node->f_ops = &pipe_file_ops;
    write_node->priv = pipe;

    vfs_file_t *read_file;
    vfs_file_t *write_file;

    assert(vfs_openat(read_node, NULL, O_RDONLY, &read_file) == 0);
    assert(vfs_openat(write_node, NULL, O_WRONLY, &write_file) == 0);

    inode_release(read_node);
    inode_release(write_node);

    // Add file descriptors to process
    assert(fd_add(read_file, &fildes[0]) == 0);
    assert(fd_add(write_file, &fildes[1]) == 0);

    return 0;
}


/**
 * @brief pipe init routine
 */
int pipe_init() {
    pipe_cache = slab_createCache("pipe node", SLAB_CACHE_DEFAULT, sizeof(fs_pipe_t), 0, pipe_initializer, pipe_deinitializer);
    return !pipe_cache;
}

FS_INIT_ROUTINE(pipe, INIT_FLAG_DEFAULT, pipe_init);