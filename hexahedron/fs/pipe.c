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

/* Pipe operations */
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
    .check_flags    = NULL
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

/* pipe endpoint access mode */
#define PIPE_ACCESS_MODE(p) ((p)->flags & O_ACCMODE)
#define IS_READ(p) (PIPE_ACCESS_MODE(p) == O_RDONLY || PIPE_ACCESS_MODE(p) == O_RDWR)
#define IS_WRITE(p) (PIPE_ACCESS_MODE(p) == O_WRONLY || PIPE_ACCESS_MODE(p) == O_RDWR)

/* utils */
#define PIPE_LOCK(p) mutex_acquire(&(p)->lock)
#define PIPE_UNLOCK(p) mutex_release(&(p)->lock)

/**
 * @brief Pipe cache object initializer
 */
static int pipe_initializer(slab_cache_t *cache, void *obj) {
    fs_pipe_t *p = obj;
    POLL_EVENT_INIT(&p->event);
    MUTEX_INIT(&p->lock);
    p->buf = ringbuffer_create(PIPE_DEFAULT_SIZE);
    p->readers = 0;
    p->writers = 0;
    p->inodes = 2;
    return 0;
}

/**
 * @brief Pipe cache object deinitializer
 */
static int pipe_deinitializer(slab_cache_t *cache, void *object) {
    fs_pipe_t *p = object;
    ringbuffer_destroy(p->buf);
    return 0;
}

/**
 * @brief pipe open
 */
static int pipe_open(vfs_file_t *file, unsigned long flags) {
    file->priv = file->inode->priv;
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;

    PIPE_LOCK(pipe);
    if (IS_READ(file)) pipe->readers++;
    if (IS_WRITE(file)) pipe->writers++;
    PIPE_UNLOCK(pipe);

    return 0;
}

/**
 * @brief pipe close
 */
static int pipe_close(vfs_file_t *file) {
    // do the actual destruction in pipe_destroy
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;

    PIPE_LOCK(pipe);
    if (IS_READ(file)) {
        assert(pipe->readers > 0);
        pipe->readers--;
        if (pipe->readers == 0) {
            poll_signal(&pipe->event, POLLERR);
        }
    }

    if (IS_WRITE(file)) {
        assert(pipe->writers > 0);
        pipe->writers--;
        if (pipe->writers == 0) {
            poll_signal(&pipe->event, POLLHUP);
        }
    }
    PIPE_UNLOCK(pipe);

    return 0;
}

/**
 * @brief pipe read
 */
static ssize_t pipe_read(vfs_file_t *file, loff_t off, size_t size, char *buffer) {
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;

    PIPE_LOCK(pipe);

    // Check for remaining space
    while (ringbuffer_remaining_read(pipe->buf) == 0 && pipe->writers > 0) {
        if (file->flags & O_NONBLOCK) {
            PIPE_UNLOCK(pipe);
            return -EAGAIN;
        }

        // TODO Avoid holding lock while allocing
        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &pipe->event, POLLIN);
        PIPE_UNLOCK(pipe);

        int wake = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);

        if (wake != 0) {
            return wake;
        }

        // Re-lock and try again
        PIPE_LOCK(pipe);
    }

    if (pipe->writers == 0 && ringbuffer_remaining_read(pipe->buf) == 0) {
        PIPE_UNLOCK(pipe);
        return 0;
    }

    ssize_t read = ringbuffer_read(pipe->buf, buffer, size);
    if (read) poll_signal(&pipe->event, POLLOUT);
    PIPE_UNLOCK(pipe);
    return read;
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
    size_t written = 0;

    if (size == 0) return 0;

    PIPE_LOCK(pipe);
    while (written < size) {
        if (pipe->readers == 0) {
            PIPE_UNLOCK(pipe);
            if (written) return (ssize_t)written;
            signal_send(current_cpu->current_process, SIGPIPE);
            return -EPIPE;
        }

        size_t needed = (size <= PIPE_ATOMIC_WRITE) ? size : 1;

        if (ringbuffer_remaining_write(pipe->buf) < needed) {
            if (file->flags & O_NONBLOCK) {
                PIPE_UNLOCK(pipe);
                return (written) ? (ssize_t)written : -EAGAIN;
            }

            // TODO Avoid holding lock while allocing
            poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
            poll_add(w, &pipe->event, POLLOUT);
            PIPE_UNLOCK(pipe);

            int wake = poll_wait(w, -1);
            poll_exit(w);
            poll_destroyWaiter(w);

            if (wake != 0) {
                return (written) ? (ssize_t)written : wake;
            }

            // Re-lock and try again
            PIPE_LOCK(pipe);
            continue;
        }

        // We hold the lock
        ssize_t chunk = ringbuffer_write(pipe->buf, (char*)buffer + written, size - written);
        if (chunk > 0) {
            written += chunk;
            poll_signal(&pipe->event, POLLIN);
        }
    }

    PIPE_UNLOCK(pipe);
    return (ssize_t)written;
}

/**
 * @brief pipe poll events
 */
static poll_events_t pipe_poll_events(vfs_file_t *file) {
    fs_pipe_t *pipe = file->priv;

    PIPE_LOCK(pipe);
    poll_events_t events = 0;

    if (IS_READ(file)) {
        events |= (ringbuffer_remaining_read(pipe->buf) ? POLLIN : 0);
        if (pipe->writers == 0) events |= POLLHUP;
    }

    if (IS_WRITE(file)) {
        events |= (ringbuffer_remaining_write(pipe->buf) ? POLLOUT : 0);
        if (pipe->readers == 0) events |= POLLERR;
    }

    PIPE_UNLOCK(pipe);
    return events;
}

/**
 * @brief pipe poll
 */
static int pipe_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events) {
    fs_pipe_t *pipe = file->priv;

    poll_add(waiter, &pipe->event, events);

    return 0;
}

/**
 * @brief pipe destroy
 */
static int pipe_destroy(vfs_inode_t *inode) {
    fs_pipe_t *pipe = inode->priv;
    
    PIPE_LOCK(pipe);
    assert(pipe->inodes > 0);
    pipe->inodes--;
    int destroy = (pipe->inodes == 0);
    PIPE_UNLOCK(pipe);

    if (destroy) slab_free(pipe_cache, pipe);
    return 0;
}

/**
 * @brief Resize a pipe's buffer
 * @param pipe The pipe to resize
 * @param size The requested capacity, rounded up to a power of two
 * @returns The new capacity, or a negative error code
 */
static long pipe_resize(fs_pipe_t *pipe, size_t size) {
    if (size == 0) return -EINVAL;
    if (size > PIPE_MAXIMUM_SIZE) return -EPERM;
    if (size < PIPE_MINIMUM_SIZE) size = PIPE_MINIMUM_SIZE;

    // round up to a power of 2, Linux does this.
    size_t capacity = PIPE_MINIMUM_SIZE;
    while (capacity < size) capacity <<= 1;

    PIPE_LOCK(pipe);

    size_t queued = ringbuffer_remaining_read(pipe->buf);
    if (capacity < queued) {
        // Would drop data
        PIPE_UNLOCK(pipe);
        return -EBUSY;
    }

    if (capacity == pipe->buf->buffer_size) {
        PIPE_UNLOCK(pipe);
        return (long)capacity;
    }

    ringbuffer_t *new_buf = ringbuffer_create(capacity);
    ringbuffer_t *old = pipe->buf;

    // copy queued data
    if (queued) {
        // !!! this is the stupidest way possible to copy the data, but im INCREDIBLY lazy and F_SETPIPE_SZ is rarely used
        char *tmp = kmalloc(queued);
        ringbuffer_read(old, tmp, queued);
        ringbuffer_write(new_buf, tmp, queued);
        kfree(tmp);
    }

    pipe->buf = new_buf;
    ringbuffer_destroy(old);

    // maybe a blocked reader can write now
    poll_signal(&pipe->event, POLLOUT);
    PIPE_UNLOCK(pipe);

    return (long)capacity;
}

/**
 * @brief Handle the pipe-specific fcntl() commands
 */
long pipe_fcntl(vfs_file_t *file, int cmd, int arg) {
    if (file->inode->f_ops != &pipe_file_ops) return -EBADF;
    fs_pipe_t *pipe = (fs_pipe_t*)file->priv;

    switch (cmd) {
        case F_GETPIPE_SZ: {
            PIPE_LOCK(pipe);
            long capacity = (long)pipe->buf->buffer_size;
            PIPE_UNLOCK(pipe);
            return capacity;
        }

        case F_SETPIPE_SZ:
            return pipe_resize(pipe, (size_t)arg);

        default:
            return -EINVAL;
    }
}

/**
 * @brief Create a new pipe set for a process
 * @param fildes The file descriptor array to fill with pipes
 * @param flags O_CLOEXEC and/or O_NONBLOCK
 * @returns Error code
 */
int pipe_create(int fildes[2], int flags) {
    fs_pipe_t *pipe = slab_allocate(pipe_cache);
    if (!pipe) return -ENOMEM;

    vfs_inode_t *read_node = vfs_inode();
    read_node->attr.type = VFS_PIPE;
    read_node->attr.ino = vfs_getNextInode();
    read_node->ops = &pipe_inode_ops;
    read_node->f_ops = &pipe_file_ops;
    read_node->priv = pipe;

    vfs_inode_t *write_node = vfs_inode();
    write_node->attr.type = VFS_PIPE;
    write_node->attr.ino = vfs_getNextInode();
    write_node->ops = &pipe_inode_ops;
    write_node->f_ops = &pipe_file_ops;
    write_node->priv = pipe;

    vfs_file_t *read_file;
    vfs_file_t *write_file;

    int status_flags = flags & O_NONBLOCK;
    assert(vfs_openat(read_node, NULL, O_RDONLY | status_flags, &read_file) == 0);
    assert(vfs_openat(write_node, NULL, O_WRONLY | status_flags, &write_file) == 0);

    inode_release(read_node);
    inode_release(write_node);

    // Add file descriptors to process
    assert(fd_add(read_file, &fildes[0]) == 0);
    assert(fd_add(write_file, &fildes[1]) == 0);

    if (flags & O_CLOEXEC) {
        assert(fd_setCloseExecute(fildes[0], true) == 0);
        assert(fd_setCloseExecute(fildes[1], true) == 0);
    }

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
