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
#include <kernel/fs/vfs.h>
#include <kernel/task/process.h>
#include <structs/circbuf.h>

/**** TYPES ****/

typedef struct fs_pipe {
    fs_node_t *read;            // Reader pipe
    fs_node_t *write;           // Writer pipe
    circbuf_t *buf;             // Pipe buffer
    uint8_t closed_read;        // Read closed
    uint8_t closed_write;       // Write closed
} fs_pipe_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new pipe set for a process
 * @param process The process to create the pipes on
 * @param fildes The file descriptor array to fill with pipes
 * @returns Error code
 */
int pipe_create(process_t *process, int fildes[2]);

/**
 * @brief Create a pipe set for usage
 * @returns Pipe object
 */
fs_pipe_t *pipe_createPipe();

#endif