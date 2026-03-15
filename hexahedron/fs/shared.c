/**
 * @file hexahedron/fs/shared.c
 * @brief Ethereal shared memory API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/shared.h>
#include <kernel/fs/devfs.h> 
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <kernel/init.h>
#include <string.h>
#include <errno.h>

/* Last used shared memory key */
static key_t shared_last_key = 0; 

/* Shared memory object hashmap */
static hashmap_t *shared_hashmap = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:SHARED", __VA_ARGS__)

/* devfs directory */
static devfs_node_t *shared_directory = NULL;

static int sharedfs_open(devfs_node_t *n, unsigned long flags);
static int sharedfs_close(devfs_node_t *n);
static int sharedfs_mmap(devfs_node_t *n, void *addr, size_t size, off_t off, mmu_flags_t flags);
static int sharedfs_munmap(devfs_node_t *n, void *addr, size_t size, off_t off);

devfs_ops_t sharedfs_ops = {
    .open = sharedfs_open,
    .close = sharedfs_close,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .lseek = NULL,
    .poll = NULL,
    .poll_events = NULL,
    .mmap = sharedfs_mmap,
    .mmap_prepare = NULL,
    .munmap = sharedfs_munmap,
};

/**
 * @brief Initialize shared memory system
 */
int shared_init() {
    shared_directory = devfs_createDirectory(devfs_root, "shared");
    shared_hashmap = hashmap_create_int("shared memory object hashmap", 20);
    return 0;
}

/**
 * @brief sharedfs open
 */
static int sharedfs_open(devfs_node_t *n, unsigned long flags) {
    shared_object_t *obj = (shared_object_t*)n->priv;
    obj->refcount++;
    return 0;
}

/**
 * @brief sharedfs close
 */
static int sharedfs_close(devfs_node_t *n) {
    LOG(DEBUG, "sharedfs_close\n");
    shared_object_t *obj = (shared_object_t*)n->priv;
    obj->refcount--;
    if (obj->refcount <= 0) {
        // We hit the bottom of our refcounts, free the object
        for (uintptr_t i = 0; i < obj->size / PAGE_SIZE; i++) {
            if (obj->blocks[i]) pmm_freePage(obj->blocks[i]);
        }

        kfree(obj->blocks);
        kfree(obj);
    }

    return 0;
}

/**
 * @brief mmap method for a shared memory object
 * @param node The node
 * @param addr The address to put the mappings at
 * @param size The size of the mappings
 * @param off The offset of the mappings
 * @returns Error code
 */
static int sharedfs_mmap(devfs_node_t *n, void *addr, size_t size, off_t off, mmu_flags_t flags) {
    shared_object_t *obj = (shared_object_t*)n->priv;

    if (off < 0) return -EINVAL;
    if ((off % PAGE_SIZE) != 0) return -EINVAL;
    if (size % PAGE_SIZE != 0) size = PAGE_ALIGN_UP(size);
    if ((size_t)off >= obj->size) return -EINVAL;
    if (off + (off_t)size > (off_t)obj->size) {
        size = obj->size - (size_t)off;
        if (size == 0) return 0;
    }

    uintptr_t start_idx = (uintptr_t)off / PAGE_SIZE;
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        uintptr_t block_idx = start_idx + (i / PAGE_SIZE);
        if (!obj->blocks[block_idx]) obj->blocks[block_idx] = pmm_allocatePage(ZONE_DEFAULT);
        arch_mmu_map(NULL, (uintptr_t)addr + i, obj->blocks[block_idx], flags);
    }

    return 0;
}

/**
 * @brief munmap method for a shared memory object
 * @param node The node
 * @param addr The address
 * @param size The size of the mappings
 * @param off The offset of the mappings
 * @returns Error code
 */
static int sharedfs_munmap(devfs_node_t *n, void *addr, size_t size, off_t off) {
    // Align size to page size
    if (size % PAGE_SIZE != 0) size = PAGE_ALIGN_UP(size);  

    vmm_unmap(addr, size); // !!!: TO BE FIXED, this is a security vulnerability!

    return 0;
}
 
/**
 * @brief Create a new shared memory object
 * @param proc The process to make a shared memory object on
 * @param size The size of the shared memory object
 * @param flags Flags for the shared memory object
 * @returns A file descriptor for the new shared memory object
 */
int sharedfs_new(process_t *proc, size_t size, int flags) {
    // TODO: Should we validate size?

    // Round size to the nearest page if needed
    if (size % PAGE_SIZE != 0) size = PAGE_ALIGN_UP(size);

    shared_object_t *obj = kzalloc(sizeof(shared_object_t));
    obj->key = shared_last_key++;
    obj->flags = flags;
    obj->refcount = 0;
    obj->size = size;
    obj->blocks = kzalloc(sizeof(uintptr_t) * (size / PAGE_SIZE));

    hashmap_set(shared_hashmap, (void*)(uintptr_t)obj->key, obj);

    char name[256];
    snprintf(name, 256, "%d", obj->key);
    assert(devfs_register(shared_directory, name, VFS_BLOCKDEVICE, &sharedfs_ops, 0, 0, obj));

    snprintf(name, 256, "/device/shared/%d", obj->key);

    vfs_file_t *f = NULL;
    int r = vfs_open(name, O_RDWR, &f);
    if (r) return r;

    obj->f = f;

    int fd_num;
    r = fd_add(f, &fd_num);
    if (r < 0) {
        vfs_close(f);
        return r;
    }

    return fd_num;
}

/**
 * @brief Get the key of a shared memory object
 * @param f The node of the shared memory object
 * @returns A key or errno
 */
key_t sharedfs_key(vfs_file_t *f) {
    // Vaildate node is a shared object node
    // !!!: EXPLOITABLE
    if ((f->inode->attr.type != VFS_BLOCKDEVICE)) return -EINVAL;
    devfs_node_t *n = f->priv;
    shared_object_t *obj = (shared_object_t*)n->priv;
    return obj->key;
}

/**
 * @brief Open an object of shared memory by the key
 * @param proc The process opening the object
 * @param key The key to use to open the shared memory object
 * @returns A file descriptor for the new shared memory object
 */
int sharedfs_openFromKey(process_t *proc, key_t key) {
    // Validate that key exists
    if (!hashmap_has(shared_hashmap, (void*)(uintptr_t)key)) return -ENOENT;
    char name[256];
    snprintf(name, 256, "/device/shared/%d", key);

    vfs_file_t *f = NULL;
    int r = vfs_open(name, O_RDWR, &f);
    if (r) return r;

    int fd_num;
    r = fd_add(f, &fd_num);
    if (r < 0) { vfs_close(f); return r; }
    return fd_num;
}

/* Init routines */
FS_INIT_ROUTINE(sharedfs, INIT_FLAG_DEFAULT, shared_init, devfs);