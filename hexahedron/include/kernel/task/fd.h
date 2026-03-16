/**
 * @file hexahedron/include/kernel/task/fd.h
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

#ifndef KERNEL_TASK_FD_H
#define KERNEL_TASK_FD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mm/vmm.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/processor_data.h>

/**** DEFINITIONS ****/

#define PROCESS_FD_BASE_AMOUNT      8
#define PROCESS_FD_EXPAND_AMOUNT    8

#define PROCESS_MAX_FDS             128

/**** TYPES ****/

typedef struct fd_table {
    mutex_t lck;
    int table_size;
    int lowest_avl;             // Lowest available fd, if set to table_size there is none.
    vfs_file_t **fds;
} fd_table_t;

/**** MACROS ****/

/* legacy!!! */
#define FD(fd) (current_cpu->current_process->fd_table->fds[fd])
#define FD_VALIDATE(fd) (current_cpu->current_process->fd_table->table_size >= fd && current_cpu->current_process->fd_table->fds[fd])

#define GET_FD_OR_ERROR(fd) ({ \
                            vfs_file_t *tmp;\
                            int ret = fd_get(fd, &tmp);\
                            if (ret != 0) { \
                                return ret; \
                            } \
                            tmp;\
                           })

#define FD_HOLD(file) file_hold(file)
#define FD_FINISH(file) file_release(file)

/**** FUNCTIONS ****/

/**
 * @brief Retrieve a file descriptor for the current process
 * @param fd The file descriptor number
 * @param out The output file
 * @returns Error code
 * 
 * The file is returned with an extra reference, use @c FD_FINISH to lose that extra reference.
 */
int fd_get(int fd_number, vfs_file_t **file);

/**
 * @brief Create a new file descriptor table
 */
fd_table_t *fd_createTable(int fd_count);

/**
 * @brief Copy one process' fd table to another process' fd table
 * @param parent The parent process
 * @param child The child process
 */
void fd_copyTable(struct process *parent, struct process *child);

/**
 * @brief Destroy a file descriptor table for a process
 * @param process Process the process to destroy the fd table for
 * @returns 0 on success
 */
int fd_destroyTable(struct process *process);

/**
 * @brief Add a new file descriptor to a process
 * @param file The file to add to the process
 * @param fd_out (Output) fd number
 * @returns Error code
 */
int fd_add(vfs_file_t *file, int *fd_out);

/**
 * @brief Remove a file descriptor from the current process
 * @param fd The file descriptor to remove
 */
int fd_remove(int fd_number);


/**
 * @brief Duplicate a file descriptor for a process
 * @param oldfd The old file descriptor to duplicate
 * @param newfd The new file descriptor to duplicate
 * @param ret Returning file descriptor
 * @returns 0 on success
 */
int fd_duplicate(int oldfd, int newfd, int *ret);

#endif