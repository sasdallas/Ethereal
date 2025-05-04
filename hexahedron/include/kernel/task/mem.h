/**
 * @file hexahedron/include/kernel/task/mem.h
 * @brief Handles process memory
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_MEM_H
#define KERNEL_TASK_MEM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <kernel/mem/vas.h>

/**** DEFINITIONS ****/

/* Minimum mmap() address that needs to be returned */
#define PROCESS_MMAP_MINIMUM            PAGE_SIZE

/**** TYPES ****/

/* Prototype */
struct process;

/**
 * @brief Process memory space mapping (by mmap())
 */
typedef struct process_mapping {
    void    *addr;
    size_t  size;
    int     flags;
    int     prot;
    int     filedes;
    off_t   off;
} process_mapping_t;

/**** FUNCTIONS ****/

/**
 * @brief Map a file into a process' memory space (mmap() equivalent)
 * @returns Negative error code
 */
void *process_mmap(void *addr, size_t len, int prot, int flags, int filedes, off_t off);

/**
 * @brief Remove a mapping from a process (faster munmap())
 * @param proc The process to remove the mapping for
 * @param map The mapping to remove from the process
 */
int process_removeMapping(struct process *proc, process_mapping_t *map);

/**
 * @brief Unmap a file from a process' memory space (munmap() equivalent)
 * @returns Negative error code
 */
int process_munmap(void *addr, size_t len);

#endif