/**
 * @file hexahedron/include/kernel/fs/pipe.h
 * @brief Ethereal pipe implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_PIPE_H
#define KERNEL_FS_PIPE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/poll.h>
#include <kernel/misc/mutex.h>
#include <structs/ringbuffer.h>
#include <kernel/fs/vfs_new.h>

/**** DEFINITIONS ****/

#define PIPE_DEFAULT_SIZE   4096
#define PIPE_MINIMUM_SIZE   4096
#define PIPE_MAXIMUM_SIZE   (1024 * 1024)

/* POSIX moment */
#define PIPE_ATOMIC_WRITE   4096

/**** TYPES ****/

typedef struct fs_pipe {
    poll_event_t event;         // Event
    mutex_t lock;               // Pipe lock
    ringbuffer_t *buf;          // Ring buffer
    volatile int readers;
    volatile int writers;
    volatile int inodes;        // Number of inodes still alive, needed as multiple inodes ref this one object
} fs_pipe_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new pipe set for a process
 * @param fildes The file descriptor array to fill with pipes
 * @param flags O_CLOEXEC and/or O_NONBLOCK
 * @returns Error code
 */
int pipe_create(int fildes[2], int flags);

/**
 * @brief Handle the pipe-specific fcntl() commands
 * @param file The file to operate on
 * @param cmd F_GETPIPE_SZ or F_SETPIPE_SZ
 * @param arg The requested capacity for F_SETPIPE_SZ
 * @returns The pipe capacity, or a negative error code (-EBADF if not a pipe)
 */
long pipe_fcntl(vfs_file_t *file, int cmd, int arg);

/**
 * @brief Create a pipe set for usage
 * @returns Pipe object
 */
fs_pipe_t *pipe_createPipe();

#endif
