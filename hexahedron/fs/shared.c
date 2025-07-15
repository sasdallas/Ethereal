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
#include <kernel/fs/kernelfs.h> 
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/pmm.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <string.h>
#include <errno.h>

/* Last used shared memory key */
static key_t shared_last_key = 0; 

/* Shared memory object hashmap */
static hashmap_t *shared_hashmap = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:SHARED", __VA_ARGS__)

/**
 * @brief Initialize shared memory system
 */
void shared_init() {
    shared_hashmap = hashmap_create_int("shared memory object hashmap", 20);
}

/**
 * @brief Open method for a shared memory object
 * @param node The node to open
 * @param flags Open flags
 */
static void sharedfs_open(fs_node_t *node, unsigned int flags) {
    shared_object_t *obj = (shared_object_t*)node->dev;
    obj->refcount++;
}

/**
 * @brief Close method for a shared memory object
 * @param node The node to close
 */
static void sharedfs_close(fs_node_t *node) {
    shared_object_t *obj = (shared_object_t*)node->dev;
    obj->refcount--;

    if (obj->refcount <= 0) {
        // We hit the bottom of our refcounts, free the object
        for (uintptr_t i = 0; i < obj->size / PAGE_SIZE; i++) {
            if (obj->blocks[i]) pmm_freeBlock(obj->blocks[i]);
        }

        LOG(INFO, "Shared memory object (key: %d) destroyed\n", obj->key);
        kfree(obj);
    }
}

/**
 * @brief mmap method for a shared memory object
 * @param node The node
 * @param addr The address to put the mappings at
 * @param size The size of the mappings
 * @param off The offset of the mappings
 * @returns Error code
 */
static int sharedfs_mmap(fs_node_t *node, void *addr, size_t size, off_t off) {
    shared_object_t *obj = (shared_object_t*)node->dev;

    // Align size to page size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);  
    if (size > obj->size) size = obj->size;

    // Start mapping
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        if (!obj->blocks[i / PAGE_SIZE]) obj->blocks[i / PAGE_SIZE] = pmm_allocateBlock();
        mem_mapAddress(NULL, obj->blocks[i / PAGE_SIZE], (uintptr_t)addr + i, (obj->flags & SHARED_READ_ONLY ? MEM_PAGE_READONLY : 0));
        // LOG(DEBUG, "Mapped PMM block %p -> %p\n", obj->blocks[i / PAGE_SIZE], (uintptr_t)addr + i);
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
static int sharedfs_munmap(fs_node_t *node, void *addr, size_t size, off_t off) {
    // Align size to page size
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);  
    
    // Start mapping
    for (uintptr_t i = 0; i < size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, (uintptr_t)addr + i, MEM_DEFAULT);
        if (pg) mem_allocatePage(pg, MEM_PAGE_NOALLOC | MEM_PAGE_NOT_PRESENT);
    }

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
    if (size % PAGE_SIZE != 0) size = MEM_ALIGN_PAGE(size);

    shared_object_t *obj = kzalloc(sizeof(shared_object_t));
    obj->key = shared_last_key++;
    obj->flags = flags;
    obj->refcount = 0;
    obj->size = size;
    obj->blocks = kzalloc(sizeof(uintptr_t) * (size / PAGE_SIZE));

    hashmap_set(shared_hashmap, (void*)(uintptr_t)obj->key, obj);

    // Make a node for this device
    // TODO: Find a better way to identify shared memory objects (?)
    fs_node_t *node = fs_node();
    strncpy(node->name, "shared memory object", 256);
    node->flags = VFS_BLOCKDEVICE;
    node->impl = SHARED_IMPL;
    node->dev = (void*)obj;
    node->atime = node->ctime = node->mtime = now();
    node->length = size;

    node->mmap = sharedfs_mmap;
    node->munmap = sharedfs_munmap;
    node->close = sharedfs_close;
    node->open = sharedfs_open;

    fs_open(node, 0);

    // Add file descriptor
    fd_t *fd = fd_add(proc, node);
    return fd->fd_number;
}

/**
 * @brief Get the key of a shared memory object
 * @param node The node of the shared memory object
 * @returns A key or errno
 */
key_t sharedfs_key(fs_node_t *node) {
    // Vaildate node is a shared object node
    if (node->flags != VFS_BLOCKDEVICE || node->impl != SHARED_IMPL) return -EINVAL;

    shared_object_t *obj = (shared_object_t*)node->dev;
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
    shared_object_t *obj = (shared_object_t*)hashmap_get(shared_hashmap, (void*)(uintptr_t)key);
    if (obj == NULL) return -ENOENT;

    // Create filesystem node
    // TODO: Find a better way to identify shared memory objects (?)
    fs_node_t *node = fs_node();
    strncpy(node->name, "shared memory object", 256);
    node->flags = VFS_BLOCKDEVICE;
    node->impl = SHARED_IMPL;
    node->dev = (void*)obj;
    node->atime = node->ctime = node->mtime = now();
    node->length = obj->size;

    node->mmap = sharedfs_mmap;
    node->munmap = sharedfs_munmap;
    node->close = sharedfs_close;
    node->open = sharedfs_open;

    fs_open(node, 0);

    // Add file descriptor
    fd_t *fd = fd_add(proc, node);
    return fd->fd_number;    
}