/**
 * @file hexahedron/include/kernel/task/fd.h
 * @brief File descriptor handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_FD_H
#define KERNEL_TASK_FD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/mem.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

#define PROCESS_FD_BASE_AMOUNT      8
#define PROCESS_FD_EXPAND_AMOUNT    8

/**** TYPES ****/

/**
 * @brief A single file descriptor. Located in a process' @c fd_table
 */
typedef struct fd {
    int fd_number;              // File descriptor number
    fs_node_t *node;            // File that this file descriptor is connected to
    mode_t mode;                // Mode of file descriptor
    uint64_t offset;            // Offset of file descriptor
} fd_t;

/**
 * @brief File descriptor table
 * @todo    Use a bitmap to keep track of free/used file descriptors? Right now the solution is sort of janky.
 *          We can also just loop through to check for a NULL entry.
 */
typedef struct fd_table {
    fd_t **fds;                 // List of expanding file descriptors
    size_t amount;              // Amount of used file descriptors
    size_t total;               // Total space for file descriptors allocated
    size_t references;          // References by other processes
    spinlock_t lock;            // Lock
} fd_table_t;

/**** MACROS ****/

#define FD(proc, fd) (proc->fd_table->fds[fd])
#define FD_VALIDATE(proc, fd) (proc->fd_table->amount >= (size_t)fd && proc->fd_table->fds[fd])

/**** FUNCTIONS ****/

/**
 * @brief Destroy a file descriptor table for a process
 * @param process Process the process to destroy the fd table for
 * @returns 0 on success
 */
int fd_destroyTable(struct process *process);

/**
 * @brief Add a file descriptor for a process
 * @param process The process to add the file descriptor to
 * @param node The node to add the file descriptor for
 * @returns A pointer to the file descriptor (for reference - it is already added to the process)
 */
fd_t *fd_add(struct process *process, fs_node_t *file);

/**
 * @brief Destroy a file descriptor for a process
 * @param process The process to take the file descriptor from
 * @param fd The file descriptor to remove
 * @returns 0 on success
 */
int fd_remove(struct process *process, int fd_number);


#endif