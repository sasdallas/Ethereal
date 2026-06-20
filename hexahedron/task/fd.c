/**
 * @file hexahedron/task/fd.c
 * @brief Task file descriptor system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:FD", __VA_ARGS__)

#define FD_TEST_CLOEXEC(tbl,fd) bitmap_test((tbl)->cloexec, fd)
#define FD_CLR_CLOEXEC(tbl,fd) bitmap_clear((tbl)->cloexec, fd)
#define FD_SET_CLOEXEC(tbl,fd) bitmap_set((tbl)->cloexec, fd)

/**
 * @brief Create a new file descriptor table
 */
fd_table_t *fd_createTable(int fd_count) {
    // TODO: does this get used enough to warrant a slab?
    fd_table_t *table = kmalloc(sizeof(fd_table_t));
    MUTEX_INIT(&table->lck);
    table->fds = kzalloc(sizeof(vfs_file_t*) * fd_count);
    table->table_size = fd_count;
    table->lowest_avl = 0;
    table->cloexec = kmalloc(BITMAP_TO_SIZE(fd_count));
    bitmap_fill(table->cloexec, 0, fd_count);
    return table;
}

/**
 * @brief Copy one process' fd table to another process' fd table
 * @param parent The parent process
 * @param child The child process
 */
void fd_copyTable(struct process *parent, struct process *child) {
    if (!parent) {
        child->fd_table = fd_createTable(PROCESS_FD_BASE_AMOUNT);
        return;
    }

    fd_table_t *source = parent->fd_table;
    mutex_acquire(&source->lck);
    
    fd_table_t *target = fd_createTable(source->table_size);
    child->fd_table = target;

    memcpy(target->cloexec, source->cloexec, BITMAP_TO_SIZE(source->table_size));
    
    target->lowest_avl = source->lowest_avl;
    for (int i = 0; i < source->table_size; i++) {
        vfs_file_t *f = source->fds[i];
        if (f) {
            file_hold(f);
            target->fds[i] = f;
        }
    }
    
    mutex_release(&source->lck);
}

/**
 * @brief Destroy a file descriptor table for a process
 * @param process Process the process to destroy the fd table for
 * @returns 0 on success
 */
int fd_destroyTable(struct process *process) {
    fd_table_t *table = process->fd_table;
    for (int i = 0; i < table->table_size; i++) {
        if (table->fds[i]) {
            vfs_close(table->fds[i]);
        }
    }

    kfree(table->cloexec);
    kfree(table->fds);
    kfree(table);
    return 0;
}

/**
 * @brief Recalculate lowest available
 */
static inline int fd_recalculateFirstFree(fd_table_t *table) {
    if (table->lowest_avl >= table->table_size) {
        return 1;    
    }

    while (table->fds[table->lowest_avl] != NULL) {
        if (table->lowest_avl >= table->table_size) {
            return 1;
        }

        table->lowest_avl++;
    }

    return 0;
}

/**
 * @brief Grow fd table
 */
static int fd_growTable(fd_table_t *table, int new_fd_count) {
    if (new_fd_count > PROCESS_MAX_FDS) {
        return -EMFILE;
    }

    int old_fd_count = table->table_size;
    table->fds = krealloc(table->fds, sizeof(vfs_file_t*) * new_fd_count);
    table->table_size = new_fd_count;

    table->cloexec = krealloc(table->cloexec, BITMAP_TO_SIZE(new_fd_count));

    if (new_fd_count > old_fd_count) {
        memset(&table->fds[old_fd_count], 0, sizeof(vfs_file_t*) * (new_fd_count - old_fd_count));
        bitmap_clear_range(table->cloexec, old_fd_count, new_fd_count);
    }

    return 0;
}

/**
 * @brief Find free file descriptor
 */
static int fd_allocateFree(fd_table_t *table) {
    if (fd_recalculateFirstFree(table)) {
        // then we need to grow the table
        int r = fd_growTable(table, table->table_size + 8);
        if (r != 0) {
            LOG(DEBUG, "Error growing table: %d\n", r);
            return r;    
        }    
    }

    int fd = table->lowest_avl;
    table->lowest_avl++;
    fd_recalculateFirstFree(table);
    return fd;
}

/**
 * @brief Get file descriptor
 * @param fd_number The file descriptor number
 * @param file The file
 */
int fd_get(int fd_number, vfs_file_t **file) {
    fd_table_t *target = current_cpu->current_process->fd_table;
    mutex_acquire(&target->lck);
    if (fd_number >= target->table_size || target->fds[fd_number] == NULL) {
        mutex_release(&target->lck);
        return -EBADF;
    }

    vfs_file_t *ret = target->fds[fd_number];
    FD_HOLD(ret);
    *file = ret;
    mutex_release(&target->lck);
    return 0;
}

/**
 * @brief Add a new file descriptor to a process
 * @param file The file to add to the process
 * @param fd_out (Output) fd number
 * @returns Error code
 */
int fd_add(vfs_file_t *file, int *fd_out) {
    fd_table_t *table = current_cpu->current_process->fd_table;
    mutex_acquire(&table->lck);

    // Allocate a new file descriptor
    int r = fd_allocateFree(table);
    if (r < 0) {
        mutex_release(&table->lck);
        return r;
    }

    // Ok set it up now
    table->fds[r] = file;
    *fd_out = r;

    // Clear the CLOEXEC flag by default
    FD_CLR_CLOEXEC(table, r);

    // Done
    mutex_release(&table->lck);
    return 0;
}

/**
 * @brief Remove a file descriptor from the current process
 * @param fd The file descriptor to remove
 */
int fd_remove(int fd_number) {
    fd_table_t *table = current_cpu->current_process->fd_table;
    mutex_acquire(&table->lck);

    if (fd_number >= table->table_size || table->fds[fd_number] == NULL) {
        mutex_release(&table->lck);
        return -EBADF;
    }

    vfs_file_t *f = table->fds[fd_number];
    table->fds[fd_number] = NULL;
    if (fd_number < table->lowest_avl) table->lowest_avl = fd_number;
    mutex_release(&table->lck);

    vfs_close(f);
    return 0;
}

/**
 * @brief Duplicate a file descriptor for a process
 * @param oldfd The old file descriptor to duplicate
 * @param newfd The new file descriptor to duplicate
 * @param ret Returning file descriptor
 * @param is_exact Whether newfd is exact or not. This is for stuff like F_DUPFD
 * @returns 0 on success
 */
int fd_duplicate(int oldfd, int newfd, int *ret, bool is_exact) {
    LOG(DEBUG, "fd_duplicate %d %d\n", oldfd, newfd);
    fd_table_t *table = current_cpu->current_process->fd_table;
    if (oldfd < 0) return -EBADF;
    mutex_acquire(&table->lck);

    if (oldfd >= table->table_size || table->fds[oldfd] == NULL) {
        mutex_release(&table->lck);
        return -EBADF;
    }

    if (newfd != -1 && newfd >= PROCESS_MAX_FDS) {
        mutex_release(&table->lck);
        return -EINVAL;
    }

    // depending on whether this is a generic duplication, do different things
    if (newfd == -1) {
        newfd = fd_allocateFree(table);
        if (newfd < 0) {
            mutex_release(&table->lck);
            return newfd;
        }
    } else {
        if (newfd >= table->table_size) {
            int r = fd_growTable(table, newfd+1);
            if (r < 0) {
                mutex_release(&table->lck);
                return r;
            }
        }

        if (!is_exact) {
            while (table->fds[newfd] != NULL) newfd++; 
        }
    }

    // Clear the cloexec flag
    FD_CLR_CLOEXEC(table, newfd);

    vfs_file_t *f = table->fds[oldfd];
    file_hold(f);

    if (table->fds[newfd]) {
        vfs_close(table->fds[newfd]);
    }

    table->fds[newfd] = f;
    *ret = newfd;
    mutex_release(&table->lck);
    return 0;
}


/**
 * @brief Tests whether a file is close-on-exec
 */
bool fd_isCloseExecute(int fd) {
    fd_table_t *tbl = current_cpu->current_process->fd_table;
    mutex_acquire(&tbl->lck);

    // validate fd
    if (fd >= tbl->table_size || tbl->fds[fd] == NULL) {
        mutex_release(&tbl->lck);
        return false;
    }

    bool t = FD_TEST_CLOEXEC(tbl,fd);

    mutex_release(&tbl->lck);
    return t;
}

/**
 * @brief Set FD_CLOEXEC flag
 */
int fd_setCloseExecute(int fd, bool state) {
    fd_table_t *tbl = current_cpu->current_process->fd_table;
    mutex_acquire(&tbl->lck);

    // validate fd
    if (fd >= tbl->table_size || tbl->fds[fd] == NULL) {
        mutex_release(&tbl->lck);
        return -EBADF;
    }

    if (state) {
        FD_SET_CLOEXEC(tbl, fd);
    } else {
        FD_CLR_CLOEXEC(tbl, fd);
    }

    mutex_release(&tbl->lck);
    return 0;
}
