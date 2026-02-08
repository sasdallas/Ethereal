/**
 * @file hexahedron/mm/mm_sysfs.c
 * @brief Memory manager SystemFS
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
#include <kernel/mm/vmm.h>
#include <stdio.h>
#include <kernel/init.h>

/* /system/memory */
static systemfs_node_t *systemfs_memory_dir = NULL;

extern slab_cache_t *slab_cache_list;

/**
 * @brief kernel pmm read
 */
ssize_t systemfs_memory_pmm(systemfs_node_t *node) {
    uintptr_t total_blocks = pmm_getTotalBlocks();
    uintptr_t used_blocks = pmm_getUsedBlocks();
    uintptr_t free_blocks = pmm_getFreeBlocks();

    return systemfs_printf(
        node,
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
ssize_t systemfs_memory_alloc(systemfs_node_t *node) {
    size_t alloc = alloc_used();
    size_t cache_count = alloc_cacheCount();

    return systemfs_printf(
        node,
        "KernelMemoryAllocator:%zu kB\n"
        "KernelMemoryAllocatorCacheCount:%d\n",
        alloc / 1000,
        cache_count
    );
}

/**
 * @brief kernel slab read
 */
ssize_t systemfs_memory_slab(systemfs_node_t *node) {
    ssize_t tot = 0;
    tot += systemfs_printf(node, "# name, mem_usage, obj_size, obj_align\n");
    slab_cache_t *i = slab_cache_list;
    while (i) {
        tot += systemfs_printf(node,
                    "%s %dkB %d %d\n", 
                        i->name,
                        (i->mem_usage / 1000),
                        i->slab_object_size,
                        i->slab_object_alignment);
        i = i->next;
    }

    return tot;
}


/**
 * @brief Initialize mm_sysfs
 */
int systemfs_memory_init() {
    assert((systemfs_memory_dir = systemfs_createDirectory(systemfs_root, "memory")));
    systemfs_registerSimple(systemfs_memory_dir, "pmm", systemfs_memory_pmm, NULL, NULL);
    systemfs_registerSimple(systemfs_memory_dir, "alloc", systemfs_memory_alloc, NULL, NULL);
    systemfs_registerSimple(systemfs_memory_dir, "slab", systemfs_memory_slab, NULL, NULL);
    return 0;
}

FS_INIT_ROUTINE(mm_sysfs, INIT_FLAG_DEFAULT, systemfs_memory_init, systemfs);