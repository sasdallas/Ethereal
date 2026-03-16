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


/**
 * @brief maps read
 */
ssize_t systemfs_proc_maps(systemfs_node_t *n) {
    process_t *p = n->priv;

    vmm_space_t *sp = p->ctx->space;
    vmm_memory_range_t *r = sp->range;
    ssize_t tot = 0;
    while (r) {
        
        // Entry format:
        // start-end rwxp size name

        tot += systemfs_printf(n,
            "%llx-%llx r%c%c%c %6d %s\n",
            r->start, r->end,
            (r->mmu_flags & MMU_FLAG_WRITE) ? 'w' : '-',
            (r->mmu_flags & MMU_FLAG_NOEXEC) ? '-' : 'x',
            (r->vmm_flags & VM_FLAG_SHARED) ? '-' : 'p',
            r->vmm_flags & VM_FLAG_FILE ? r->file.offset : 0,
            r->vmm_flags & VM_FLAG_FILE ? r->file.path : "[anonymous]");
        
        r = r->next;
    }

    return tot;
}

/**
 * @brief mem_usage read
 */
ssize_t systemfs_proc_mem_usage(systemfs_node_t *n) {
    process_t *p = n->priv;

    vmm_metrics_t *met = &p->ctx->space->metrics;
    return systemfs_printf(n,
        "TotalMemoryUsage:%d kB\n"
        "TotalMemoryResident:%d kB\n"
        "AnonUsage:%d kB\n"
        "AnonResident:%d kB\n"
        "FileUsage:%d kB\n"
        "FileResident:%d kB\n",
        (met->anon_usage + met->file_usage) / 1024,
        (met->anon_resident + met->file_resident) / 1024,
        (met->anon_usage) / 1024,
        (met->anon_resident) / 1024,
        (met->file_usage) / 1024,
        (met->file_resident) / 1024);
}

/**
 * @brief Status
 */
ssize_t systemfs_proc_status(systemfs_node_t *n) {
    process_t *p = n->priv;
    return systemfs_printf(n,
        "ProcessName:%s\n"
        "Pid:%d\n"
        "Pgid:%d\n"
        "Uid:%d %d\n"
        "Gid:%d %d\n"
        "Sid:%d",
        p->name, p->pid, p->pgid,
        p->uid, p->euid, p->gid, p->egid,
        p->sid);
}

/**
 * @brief Create process systemfs node
 * @param proc The process to creat eit for
 */
systemfs_node_t *systemfs_proc_create(process_t *proc) {
    // !!! I hate this entire fucking function
extern slab_cache_t *systemfs_node_cache;
    if (!systemfs_node_cache) return NULL; // To stop the idle process from trying to allocate bullshit...
    systemfs_node_t *n = systemfs_node();
    n->attr.type = VFS_DIRECTORY;
    n->name = "proc"; // n->name is never actually used
    n->children = hashmap_create("proc children", 10);
    n->priv = proc;

    systemfs_registerSimple(n, "status", systemfs_proc_status, NULL, proc);
    systemfs_registerSimple(n, "maps", systemfs_proc_maps, NULL, proc);
    systemfs_registerSimple(n, "mem_usage", systemfs_proc_mem_usage, NULL, proc);
    return n;
}

/**
 * @brief Destroy process SystemFS node
 */
void systemfs_proc_destroy(process_t *proc) {
    systemfs_node_t *n = proc->proc_sysfs;
    systemfs_unregister(n, "status");
    systemfs_unregister(n, "maps");
    systemfs_unregister(n, "mem_usage");
    systemfs_free(n);
}


/**
 * @brief proc dir lookup
 */
static int systemfs_proc_lookup(systemfs_node_t *node, char *name, systemfs_node_t **output) {
    process_t *proc = NULL;
    if (!strcmp(name, "self")) {
        proc = current_cpu->current_process;    
    } else {
        proc = process_getFromPID(strtol(name, NULL, 10));
    }

    if (!proc || proc->pid == -1) return -ENOENT;


    *output = proc->proc_sysfs;
    return 0;
}

/**
 * @brief proc dir read entry
 */
static int systemfs_proc_read_entry(systemfs_node_t *node, vfs_dir_context_t *ctx) {
    // first entry is self
    if (ctx->dirpos == 2) {
        strncpy(ctx->name, "self", NAME_MAX);
        ctx->ino = 1; // !!!
        return 0;
    }

    int i = ctx->dirpos - 3;
    int j = 0;

    // !!!: unsafe without RCU
extern list_t *process_list;
    foreach(procnode, process_list) {
        process_t *proc = procnode->value;
        if (proc->pid < 0) continue;
        if (i == j) {
            snprintf(ctx->name, NAME_MAX, "%ld", proc->pid);
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
ssize_t systemfs_kernel_cmdline(systemfs_node_t *node) {
    return systemfs_printf(node,
        "%s\n", arch_get_generic_parameters()->kernel_cmdline
    );
}

/**
 * @brief kernel uptime read
 */
ssize_t systemfs_kernel_uptime(systemfs_node_t *node) {
    unsigned long seconds, subseconds;
    clock_relative(0, 0, &seconds, &subseconds);
    return systemfs_printf(node,
        "%lu.%016lu\n", seconds, subseconds
    );
}

/**
 * @brief systemfs_kernel_init
 */
static int systemfs_kernel_init() {
    systemfs_registerSimple(systemfs_root, "cmdline", systemfs_kernel_cmdline, NULL, NULL);
    systemfs_registerSimple(systemfs_root, "uptime", systemfs_kernel_uptime, NULL, NULL);
    systemfs_register(systemfs_root, "processes", VFS_DIRECTORY, &systemfs_proc_dir_ops, NULL);

    return 0;
}

FS_INIT_ROUTINE(systemfs_kernel, INIT_FLAG_DEFAULT, systemfs_kernel_init, systemfs);