/**
 * @file hexahedron/fs/systemfs_kernel.c
 * @brief Provides some barebones SystemFS kernel entries
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/systemfs.h>
#include <kernel/drivers/clock.h>
#include <kernel/config.h>
#include <kernel/init.h>
#include <kernel/task/process.h>
#include <kernel/mm/vmm.h>
#include <stdlib.h>
#include <stdio.h>

static int systemfs_proc_read_entry(systemfs_node_t *node, vfs_dir_context_t *ctx);
static int systemfs_proc_lookup(systemfs_node_t *node, char *name, systemfs_node_t **output);
static systemfs_ops_t systemfs_proc_dir_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .read_entry = systemfs_proc_read_entry,
    .lookup = systemfs_proc_lookup,
};

/* /system/memory */
static systemfs_node_t *systemfs_memory_dir = NULL;

/**
 * @brief proc dir lookup
 */
static int systemfs_proc_lookup(systemfs_node_t *node, char *name, systemfs_node_t **output) {
    pid_t tp = 0;

    if (!strcmp(name, "self")) {
        tp = current_cpu->current_process->pid;    
    } else {
        tp = strtol(name, NULL, 10);
    }

    process_t *target = process_getFromPID(tp);
    return -ENOENT;
}

/**
 * @brief proc dir read entry
 */
static int systemfs_proc_read_entry(systemfs_node_t *node, vfs_dir_context_t *ctx) {
    // first entry is self
    if (ctx->dirpos == 2) {
        ctx->name = "self";
        ctx->ino = 1; // !!!
        return 0;
    }

    int i = ctx->dirpos - 3;
    int j = 0;

    // !!!: unsafe without RCU
extern list_t *process_list;
    foreach(procnode, process_list) {
        if (i == j) {
            process_t *proc = procnode->value;
            
            // !!!: LEAKING MEMORY EVERY SINGLE TIME THIS IS CALLED
            char tmp[50];
            snprintf(tmp, 50, "%d", proc->pid);
            ctx->name = strdup(tmp);
            ctx->ino = proc->pid; // !!!
            return 0;
        }
        
        j++;
    }

    return 1;
}

/**
 * @brief kernel cmdline read
 */
ssize_t systemfs_kernel_cmdline(systemfs_node_t *node, loff_t off, size_t size, char *buffer) {
    return systemfs_printf(
        buffer, off, size,
        "%s\n", arch_get_generic_parameters()->kernel_cmdline
    );
}

/**
 * @brief kernel uptime read
 */
ssize_t systemfs_kernel_uptime(systemfs_node_t *node, loff_t off, size_t size, char *buffer) {
    unsigned long seconds, subseconds;
    clock_relative(0, 0, &seconds, &subseconds);
    return systemfs_printf(
        buffer, off, size,
        "%lu.%016lu\n", seconds, subseconds
    );
}

/**
 * @brief kernel pmm read
 */
ssize_t systemfs_memory_pmm(systemfs_node_t *node, loff_t off, size_t size, char *buffer) {
    uintptr_t total_blocks = pmm_getTotalBlocks();
    uintptr_t used_blocks = pmm_getUsedBlocks();
    uintptr_t free_blocks = pmm_getFreeBlocks();

    return systemfs_printf(
        buffer, off, size,
        "TotalPhysBlocks:%d\n"
        "TotalPhysMemory:%zu kB\n"
        "UsedPhysMemory:%zu kB\n"
        "FreePhysMemory:%zu kB\n",
            total_blocks,
            total_blocks * PAGE_SIZE / 1000,
            used_blocks * PAGE_SIZE / 1000,
            free_blocks * PAGE_SIZE / 1000
    );
}

/**
 * @brief kernel alloc read
 */
ssize_t systemfs_memory_alloc(systemfs_node_t *node, loff_t off, size_t size, char *buffer) {
    size_t alloc = alloc_used();
    size_t cache_count = alloc_cacheCount();

    return systemfs_printf(
        buffer, off, size,
        "KernelMemoryAllocator:%zu kB\n"
        "KernelMemoryAllocatorCacheCount:%d\n",
        alloc / 1000,
        cache_count
    );
}

/**
 * @brief systemfs_kernel_init
 */
static int systemfs_kernel_init() {
    systemfs_registerSimple(systemfs_root, "cmdline", systemfs_kernel_cmdline, NULL, NULL);
    systemfs_registerSimple(systemfs_root, "uptime", systemfs_kernel_uptime, NULL, NULL);
    systemfs_register(systemfs_root, "processes", VFS_DIRECTORY, &systemfs_proc_dir_ops, NULL);
    assert((systemfs_memory_dir = systemfs_createDirectory(systemfs_root, "memory")));
    systemfs_registerSimple(systemfs_memory_dir, "pmm", systemfs_memory_pmm, NULL, NULL);
    systemfs_registerSimple(systemfs_memory_dir, "alloc", systemfs_memory_alloc, NULL, NULL);

    return 0;
}

FS_INIT_ROUTINE(systemfs_kernel, INIT_FLAG_DEFAULT, systemfs_kernel_init, systemfs);