/**
 * @file hexahedron/task/fd.c
 * @brief File descriptor handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/task/fd.h>
#include <kernel/mem/alloc.h>
#include <string.h>
#include <kernel/debug.h>

/**
 * @brief Destroy a file descriptor table for a process
 * @param process Process the process to destroy the fd table for
 * @returns 0 on success
 */
int fd_destroyTable(struct process *process) {
    if (!process->fd_table) return 1;

    spinlock_acquire(&process->fd_table->lock);
    // Does the process fd table still have any references?
    if (process->fd_table->references > 1) {
        // Yes. Decrement and NULL
        process->fd_table->references--;
        spinlock_release(&process->fd_table->lock);
        process->fd_table = NULL;
        return 0;
    }

    // No, we can free this now.
    // For each file descriptor we'll free the memory
    for (size_t i = 0; i < process->fd_table->amount; i++) {
        fd_t *fd = process->fd_table->fds[i];
        if (fd) {
            fs_close(fd->node);
            kfree(fd);
        }
    }

    kfree(process->fd_table->fds);
    spinlock_release(&process->fd_table->lock);
    kfree(process->fd_table);
    process->fd_table = NULL;

    return 0;
}

/**
 * @brief Add a file descriptor for a process
 * @param process The process to add the file descriptor to
 * @param node The node to add the file descriptor for
 * @returns A pointer to the file descriptor (for reference - it is already added to the process)
 * 
 * @note You should increment the file's refcount yourself
 */
fd_t *fd_add(struct process *process, fs_node_t *node) {
    if (!process || !node) return NULL;

    spinlock_acquire(&process->fd_table->lock);

    // First, make sure the process' has enough file descriptors
    if (process->fd_table->total <= process->fd_table->amount) {
        // Reallocate to a new capacity, adding PROCESS_FD_EXPAND_AMOUNT
        process->fd_table->total += PROCESS_FD_EXPAND_AMOUNT;
        process->fd_table->fds = krealloc(process->fd_table->fds, sizeof(fd_t*) * process->fd_table->total);
        memset(&process->fd_table->fds[process->fd_table->total-PROCESS_FD_EXPAND_AMOUNT], 0, PROCESS_FD_EXPAND_AMOUNT * sizeof(fd_t*));
    }

    // Search through the file descriptor list to find a spot
    for (unsigned int i = 0; i < process->fd_table->total; i++) {
        if (!process->fd_table->fds[i]) {
             // Allocate a new fd
            fd_t *new_fd = kmalloc(sizeof(fd_t));
            memset(new_fd, 0, sizeof(fd_t));
            new_fd->fd_number = i;
            new_fd->node = node;
            process->fd_table->fds[i] = new_fd;
            process->fd_table->amount++;
            
            spinlock_release(&process->fd_table->lock);
            return new_fd;
        }
    }

    // ???
    dprintf_module(ERR, "TASK:FD", "CRITICAL: Corrupted file descriptor bitmap. Could not find a spot\n");
    return NULL;
}

/**
 * @brief Destroy a file descriptor for a process
 * @param process The process to take the file descriptor from
 * @param fd The file descriptor to remove
 * @returns 0 on success
 */
int fd_remove(struct process *process, int fd_number) {
    if (!process || !process->fd_table) return 1;
    if ((size_t)fd_number > process->fd_table->total) return 1;

    spinlock_acquire(&process->fd_table->lock);

    // Get the file descriptor
    fd_t *fd = process->fd_table->fds[fd_number];
    fs_close(fd->node);
    kfree(fd);
    process->fd_table->fds[fd_number] = NULL;

    process->fd_table->amount--;


    spinlock_release(&process->fd_table->lock);

    return 0;
}