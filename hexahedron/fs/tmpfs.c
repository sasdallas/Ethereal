/**
 * @file hexahedron/fs/tmpfs.c
 * @brief Provides the temporary filesystem driver
 * 
 * For LiveCD boots of Ethereal, this is provided as the root filesystem.
 * The implementation isn't perfect but functions decently alright and can conserve memory.
 * 
 * @todo Perhaps we should avoid putting files as tree nodes - maybe we can separate directories and files sort of similar to how the VFS does it.
 * @todo A bit messy, but luckily abstracted so can be fixed up :D
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/tmpfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Size rounding */
#define TMPFS_ROUND_SIZE(sz) ((sz % TMPFS_BLOCK_SIZE == 0) ? sz : (sz + TMPFS_BLOCK_SIZE - (sz % TMPFS_BLOCK_SIZE)))

/* Debug output */
#define LOG(status, ...) dprintf_module(status, "FS:TMPFS", __VA_ARGS__)

/* Function prototypes */
void tmpfs_open(fs_node_t *node, unsigned int flags);
ssize_t tmpfs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);
ssize_t tmpfs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);
void tmpfs_close(fs_node_t *node);
fs_node_t *tmpfs_finddir(fs_node_t *node, char *path);
struct dirent *tmpfs_readdir(fs_node_t *node, unsigned long index);
fs_node_t *tmpfs_create(fs_node_t *node, char *path, mode_t mode);
int tmpfs_mkdir(fs_node_t *node, char *path, mode_t mode);

/**
 * @brief Convert a temporary filesystem object into a VFS node
 * @param t The temporary filesystem object
 */
static fs_node_t *tmpfs_convertVFS(tmpfs_entry_t *t) {
    fs_node_t *node = kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));

    strncpy(node->name, t->name, 256);

    // Get t->type
    switch (t->type) {
        case TMPFS_FILE:
            node->flags = VFS_FILE;
            break;
        
        case TMPFS_DIRECTORY:
            node->flags = VFS_DIRECTORY;
            break;

        case TMPFS_SYMLINK:
            node->flags = VFS_SYMLINK;
            break;

        default:
            node->flags = VFS_FILE;
            break;
    }

    // Setup other fields
    node->mask = t->mask;
    node->uid = t->uid;
    node->gid = t->gid;
    node->atime = t->atime;
    node->mtime = t->mtime;
    node->ctime = t->ctime;
    node->dev = (void*)t;

    // Setup methods
    if (t->type == TMPFS_FILE) {
        node->length = t->file->length;
        node->open = tmpfs_open;
        node->close = tmpfs_close;
        node->read = tmpfs_read;
        node->write = tmpfs_write;
    } else if (t->type == TMPFS_DIRECTORY) {
        node->create = tmpfs_create;
        node->readdir = tmpfs_readdir;
        node->finddir = tmpfs_finddir;
        node->mkdir = tmpfs_mkdir;
    }

    return node;
}


/**
 * @brief Create a new tmpfs entry
 * @param parent The parent of the tmpfs entry
 * @param type The type of tmpfs entry to create
 * @param name The name to assign to the entry
 */
static tmpfs_entry_t *tmpfs_createEntry(tmpfs_entry_t *parent, int type, char *name) {
    tmpfs_entry_t *entry = kmalloc(sizeof(tmpfs_entry_t));
    memset(entry, 0, sizeof(tmpfs_entry_t));
    snprintf(entry->name, 256, "%s", name);
    entry->type = type;
    entry->atime = entry->ctime = entry->mtime = now();
    entry->mask = 0777; // Default mask, can be changed easily
    

    // If file, allocate file object as well
    if (type == TMPFS_FILE) {
        entry->file = kmalloc(sizeof(tmpfs_file_t));
        memset(entry->file, 0, sizeof(tmpfs_file_t));
        entry->file->lock = spinlock_create("tmpfs lock");
    } 

    if (parent) {
        entry->tree = parent->tree;
        entry->tnode = tree_insert_child(parent->tree, parent->tnode, (void*)entry);
    } else {
        entry->tree = tree_create("tmpfs tree");
        tree_set_parent(entry->tree, (void*)entry);
        entry->tnode = entry->tree->root;
    }

    return entry;
}

/**
 * @brief Temporary filesystem open method
 */
void tmpfs_open(fs_node_t *node, unsigned int flags) {
}

/**
 * @brief Temporary filesystem close method
 */
void tmpfs_close(fs_node_t *node) {
}

/**
 * @brief Temporary filesystem read method
 */
ssize_t tmpfs_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;
    if (!entry->file || !entry->file->blk_count) return 0;

    // First, get a lock on the file
    spinlock_acquire(entry->file->lock);

    size_t available_size = entry->file->blk_count * TMPFS_BLOCK_SIZE;

    // Do some VFS calculations
    if (off >= (off_t)available_size) {
        spinlock_release(entry->file->lock);
        return 0;
    }

    if (off+size > available_size) {
        size = available_size - off;
    }

    // Now, let's read
    memcpy(buffer, entry->file->blk_start + off, size);

    spinlock_release(entry->file->lock);
    return size;
}

/**
 * @brief Temporary filesystem write method
 */
ssize_t tmpfs_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;
    if (!entry->file) return 0;

    // First, get a lock on the file
    spinlock_acquire(entry->file->lock);

    // Let's see if we need to expand the file
    size_t needed_size = off + size;
    size_t available_size = entry->file->blk_count * TMPFS_BLOCK_SIZE;

    if (needed_size > available_size) {
        // Round to TMPFS_BLOCK_SIZE
        needed_size = TMPFS_ROUND_SIZE(needed_size);
        if (entry->file->blk_start) {
            entry->file->blk_start = krealloc(entry->file->blk_start, needed_size);
            entry->file->blk_count = needed_size / TMPFS_BLOCK_SIZE;
        } else {
            entry->file->blk_start = kmalloc(needed_size);
            entry->file->blk_count = needed_size / TMPFS_BLOCK_SIZE;
        }

        entry->file->length = off + size;
    }

    LOG(DEBUG, "%s: now using %d blocks - %p - %p\n", entry->name, entry->file->blk_count, entry->file->blk_start, entry->file->blk_start + (entry->file->blk_count * TMPFS_BLOCK_SIZE));
    
    // Ok, we should have enough size
    memcpy(entry->file->blk_start + off, buffer, size);



    spinlock_release(entry->file->lock);
    return size;
}

/**
 * @brief Temporary filesystem create method
 */
fs_node_t *tmpfs_create(fs_node_t *node, char *path, mode_t mode) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;

    // Try to make a new tmpfs entry
    tmpfs_entry_t *new = tmpfs_createEntry(entry, TMPFS_FILE, path);
    new->file->parent = node;

    // Return node
    return tmpfs_convertVFS(new);
}

/**
 * @brief Temporary filesystem find directory method
 */
fs_node_t *tmpfs_finddir(fs_node_t *node, char *path) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;
    tree_node_t *tnode = entry->tnode;

    foreach(child_node, tnode->children) {
        tmpfs_entry_t *target = (tmpfs_entry_t*)((tree_node_t*)child_node->value)->value;
        if (!strncmp(target->name, path, 256)) {
            // Match!
            return tmpfs_convertVFS(target);
        }
    }

    return NULL;
}

/**
 * @brief Temporary filesystem read directory method
 */
struct dirent *tmpfs_readdir(fs_node_t *node, unsigned long index) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;
    tree_node_t *tnode = entry->tnode;

    // First, handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    // Send the index
    // TODO: ugly..
    unsigned long i = 0;
    foreach(child_node, tnode->children) {
        if (i == index) {
            tmpfs_entry_t *target = (tmpfs_entry_t*)((tree_node_t*)child_node->value)->value;

            struct dirent *out = kmalloc(sizeof(struct dirent));
            strncpy(out->d_name, target->name, 256);
            out->d_ino = index;
            return out;
        }

        i++;
    }

    return NULL;
}

/**
 * @brief Temporary filesystem make directory method
 */
int tmpfs_mkdir(fs_node_t *node, char *path, mode_t mode) {
    tmpfs_entry_t *entry = (tmpfs_entry_t*)node->dev;

    // Try to make a new tmpfs entry
    tmpfs_createEntry(entry, TMPFS_DIRECTORY, path);

    // Return
    return 0;
}

/**
 * @brief Mount method for tmpfs
 * @param argp Arguments, aka name ;)
 * @returns The temporary filesystem
 */
fs_node_t *tmpfs_mount(char *argp, char *mountpoint) {
    tmpfs_entry_t *root = tmpfs_createEntry(NULL, TMPFS_DIRECTORY, argp);

    return tmpfs_convertVFS(root);
}

/**
 * @brief Initialize the temporary filesystem handler
 */
void tmpfs_init() {
    vfs_registerFilesystem("tmpfs", tmpfs_mount);
}
