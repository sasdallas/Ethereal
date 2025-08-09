/**
 * @file hexahedron/fs/pipe.c
 * @brief Ethereal pipe implementation
 * 
 * This is just a simple UNIX pipe implementation.
 * 
 * @note The closing idea of using @c closed_read and @c closed_write was sourced from klange
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/pipe.h>
#include <kernel/task/fd.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/**
 * @brief Read from a pipe
 * @param node The pipe node to read from
 * @param off The offset of the read
 * @param size The size of the read
 * @param buffer The buffer to read to
 */
ssize_t pipe_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;

    // Do we have anything left to read?
    if (!circbuf_remaining_read(pipe->buf)) return 0;

    // Start reading from the circular buffer
    return circbuf_read(pipe->buf, size, buffer);
}

/**
 * @brief Write to a pipe
 * @param node The pipe node to read from
 * @param off The offset of the read
 * @param size The size of the read
 * @param buffer The buffer to read to
 */
ssize_t pipe_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;

    // Was the read end closed?
    if (pipe->closed_read) {
        // Yes, send SIGPIPE
        signal_send(current_cpu->current_process, SIGPIPE);
        return -EPIPE;
    }

    // Start writing to the circular buffer
    return circbuf_write(pipe->buf, size, buffer);
}

/**
 * @brief Close the read end of a pipe
 * @param node The pipe node to close
 */
int pipe_closeRead(fs_node_t *node) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;

    // Did the other end close?
    // TODO: Lock this?
    if (pipe->closed_write) {
        // Yes, free the pipe object now
        circbuf_destroy(pipe->buf);
        kfree(pipe);
        return 0;
    }

    // Not yet
    pipe->closed_read = 1;
    circbuf_stop(pipe->buf);
    return 0;
}

/**
 * @brief Close the write end of a pipe
 * @param node The pipe node to close
 */
int pipe_closeWrite(fs_node_t *node) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;

    // Did the other end close?
    if (pipe->closed_read) {
        // Yes, free the pipe object now
        circbuf_destroy(pipe->buf);
        kfree(pipe);
        return 0;
    }

    // Not yet
    pipe->closed_write = 1;
    circbuf_stop(pipe->buf);
    return 0;
}

/**
 * @brief Ready to read method for a pipe
 * @param node The node to check
 * @param ready The ready flags
 */
int pipe_readyRead(fs_node_t *node, int ready) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;
    if (circbuf_remaining_read(pipe->buf)) return VFS_EVENT_READ;
    return 0;
}

/**
 * @brief Ready to write method for a pipe
 * @param node The node to check
 * @param ready The ready flags
 */
int pipe_readyWrite(fs_node_t *node, int ready) {
    fs_pipe_t *pipe = (fs_pipe_t*)node->dev;
    if (circbuf_remaining_write(pipe->buf)) return VFS_EVENT_WRITE;
    return 0;
}

/**
 * @brief Create a pipe set for usage
 * @returns Pipe object
 */
fs_pipe_t *pipe_createPipe() {
    fs_pipe_t *pipe = kzalloc(sizeof(fs_pipe_t));
    pipe->buf = circbuf_create("pipe", 4096);

    // Create read end of the pipe
    pipe->read = fs_node();
    strncpy(pipe->read->name, "read pipe", 256);
    pipe->read->flags = VFS_PIPE;
    pipe->read->mask = 0666;
    pipe->read->dev = (void*)pipe;
    pipe->read->read = pipe_read;
    pipe->read->close = pipe_closeRead;
    pipe->read->ready = pipe_readyRead;
    fs_open(pipe->read, 0);

    // Create write end of the pipe
    pipe->write = fs_node();
    strncpy(pipe->write->name, "write pipe", 256);
    pipe->write->flags = VFS_PIPE;
    pipe->write->mask = 0666;
    pipe->write->dev = (void*)pipe;
    pipe->write->write = pipe_write;
    pipe->write->close = pipe_closeWrite;
    pipe->write->ready = pipe_readyWrite;
    fs_open(pipe->write, 0);
    
    return pipe;
}

/**
 * @brief Create a new pipe set for a process
 * @param process The process to create the pipes on
 * @param fildes The file descriptor array to fill with pipes
 * @returns Error code
 */
int pipe_create(process_t *process, int fildes[2]) {
    fs_pipe_t *pipe = pipe_createPipe();

    // Add file descriptors to process
    fd_t *read_fd = fd_add(process, pipe->read);
    fd_t *write_fd = fd_add(process, pipe->write);

    // Set file descriptor numbers
    fildes[0] = read_fd->fd_number;
    fildes[1] = write_fd->fd_number;

    return 0;
}

/**
 * @brief Returns the amount of content available to read
 * @param pipe One end of a pipe
 */
size_t pipe_remainingRead(fs_node_t *pipe) {
    fs_pipe_t *p = (fs_pipe_t*)pipe->dev;
    return circbuf_remaining_read(p->buf);
}