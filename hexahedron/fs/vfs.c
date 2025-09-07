/**
 * @file hexahedron/fs/vfs.c
 * @brief Virtual filesystem handler
 * 
 * @warning Some code in here can be pretty messy.
 * @todo Some errno support would be really helpful.. could implement a @c vfs_getError system
 * 
 * @note Some code in this design, specifically design of @c vfs_mount and @c vfs_canonicalizePath was inspired by ToaruOS. 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/vfs.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <kernel/panic.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/misc/spinlock.h>
#include <kernel/processor_data.h>
#include <kernel/misc/util.h>

#include <structs/tree.h>
#include <structs/hashmap.h>
#include <structs/list.h>


/* Main VFS tree */
tree_t *vfs_tree = NULL;

/* Hashmap of filesystems (quick access) */
hashmap_t *vfs_filesystems = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:VFS", __VA_ARGS__)

/* Locks */
spinlock_t *vfs_lock;

/* Old reduceOS implemented a CWD system, but that was just for the kernel CLI */

/**
 * @brief Standard POSIX open call
 * @param node The node to open
 * @param flags The open flags
 */
int fs_open(fs_node_t *node, unsigned int flags) {
    if (!node) return -EINVAL;

    // TODO: Locking?
    node->refcount++;
    
    if (node->open) {
        return node->open(node, flags);
    }

    return 0;
}

/**
 * @brief Standard POSIX close call that also frees the node
 * @param node The node to close
 */
int fs_close(fs_node_t *node) {
    if (!node) return -EINVAL;

    // First, decrement the reference counter
    node->refcount--;

    // Anyone still using this node?
    if (node->refcount <= 0) {
        // Nope. It's free memory.
        if (node->close) {
            int r = node->close(node);
            if (r != 0) return r;
        }

        fs_destroy(node);
    }

    return 0;
}

/**
 * @brief Standard POSIX read call
 * @param node The node to read from
 * @param offset The offset to read at
 * @param size The amount of bytes to read
 * @param buffer The buffer to store the bytes in
 * @returns The amount of bytes read
 */
ssize_t fs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!node) return 0;

    if (node->read) {
        return node->read(node, offset, size, buffer);
    }

    return 0;
}

/**
 * @brief Standard POSIX write call
 * @param node The node to write to
 * @param offset The offset to write at
 * @param size The amount of bytes to write
 * @param buffer The buffer of the bytes to write
 * @returns The amount of bytes written
 */
ssize_t fs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!node) return 0;

    if (node->write) {
        return node->write(node, offset, size, buffer);
    }

    return 0;
}

/**
 * @brief Read directory
 * @param node The node to read the directory of
 * @param index The index to read from
 * @returns A directory entry structure or NULL
 */
struct dirent *fs_readdir(fs_node_t *node, unsigned long index) {
    if (!node) return NULL;

    if (node->flags & VFS_DIRECTORY && node->readdir) {
        return node->readdir(node, index);
    }

    return NULL;
}

/**
 * @brief Find directory
 * @param node The node to find the path in
 * @param name The name of the file to look for
 * @returns The node of the file found or NULL
 */
fs_node_t *fs_finddir(fs_node_t *node, char *path) {
    if (!node) return NULL;

    if (node->flags & VFS_DIRECTORY && node->finddir) {
        return node->finddir(node, path);
    }

    return NULL;
}


/**
 * @brief Read the link of the symlink
 * @param node The node to read the link of
 * @param buf A buffer to hold the symlink path in
 * @param size The size of the buffer
 */
int fs_readlink(fs_node_t *node, char *buf, size_t size) {
    if (!node) return -EINVAL;

    if (node->flags & VFS_SYMLINK && node->readlink) {
        return node->readlink(node, buf, size);
    }

    return -EINVAL;
}

/**
 * @brief Create new entry
 * @param node The node to run create() on
 * @param name The name of the new entry to create
 * @param mode The mode
 * @param node_out Output node
 */
int fs_create(fs_node_t *node, char *name, mode_t mode, fs_node_t **node_out) {
    if (!node) return -EINVAL;

    if (node->flags == VFS_DIRECTORY && node->create) {
        return node->create(node, name, mode, node_out);
    }

    return -EINVAL;
}

/**
 * @brief I/O control file
 * @param node The node to ioctl
 * @param request The ioctl request to use
 * @param argp Arguments to ioctl call
 * @returns Error code
 */
int fs_ioctl(fs_node_t *node, unsigned long request, char *argp) {
    if (!node) return -EINVAL;

    if (node->ioctl) {
        return node->ioctl(node, request, argp);
    } else {
        return -ENOTSUP;
    }
}

/**
 * @brief Check if file is ready
 * @param node The node to check
 * @param event_type OR bitmask of events (VFS_EVENT_...) to check
 * @returns OR bitmask of events the file is ready for
 */
int fs_ready(fs_node_t *node, int event_type) {
    if (!node) return -EINVAL;

    if (node->ready) {
        return node->ready(node, event_type);
    } else {
        return event_type; // Assume the node is always ready
    }
}

/**
 * @brief Alert any processes in the queue that new events are ready
 * @param node The node to alert on
 * @param events The events to alert on
 * @returns 0 on success
 */
int fs_alert(fs_node_t *node, int events) {
    if (!events) return 0;
    if (!node) return -EINVAL;
    if (!node->waiting_nodes) return 0;
    
    spinlock_acquire(&node->waiter_lock);

    // LOG(INFO, "fs_alert %s pending %d\n", node->name, node->waiting_nodes->length);

    node_t *n = node->waiting_nodes->head;
    while (n) {
        vfs_waiter_t *w = (vfs_waiter_t*)n->value;
        vfs_waiter_thread_t *wt = w->thr;   

        mutex_acquire(wt->mutex);

        // !!!
        // LOG(DEBUG, "Node %p %s: Waiting for events %x and have %x has_woken_up %ld refcount %ld thread %p\n", node, node->name, w->events, events, wt->has_woken_up, wt->refcount, wt->thread);
        if (w->events & events && !wt->has_woken_up) {
            // We have events available
            if (wt->thread->waiter != wt) {
                // Likely a timeout expired
            } else {
                sleep_wakeup(w->thr->thread);
            }

            wt->thread->waiter = NULL;
            atomic_store_explicit(&(w->thr->has_woken_up), 1, memory_order_release);
        }

        if (atomic_load_explicit(&w->thr->has_woken_up, memory_order_acquire)) {
            atomic_fetch_sub_explicit(&w->thr->refcount, 1, memory_order_acq_rel);

            // Remove this waiter from the list
            node_t *new = n->next;
            list_delete(node->waiting_nodes, n);
            n = new;        
            kfree(w);

            // Any remaining references on this node?
            if (atomic_load_explicit(&w->thr->refcount, memory_order_acquire) == 0) {
                // No references remaining!
                // !!!: If a waiter never alerts, it will never destroy.
                mutex_destroy(wt->mutex);
                kfree(wt);
            } else {
                mutex_release(wt->mutex);
            }

            continue;
        }

        // Exit non-critical
        mutex_release(wt->mutex);

        n = n->next;
    }

    spinlock_release(&node->waiter_lock);

    return 0;
}

/**
 * @brief Wait for a node to have events ready for a process
 * @param node The node to wait for events on
 * @param events The events that are being waited for
 * @returns 0 on success
 * 
 * @note Does not actually put you to sleep. Instead puts you in the queue. for sleeping
 */
int fs_wait(fs_node_t *node, int events) {
    if (!node) return -EINVAL;

    spinlock_acquire(&node->waiter_lock);
    if (!node->waiting_nodes) node->waiting_nodes = list_create("waiting nodes");

    if (!current_cpu->current_thread->waiter) {
        // Create a new thread waiter
        vfs_waiter_thread_t *w = kzalloc(sizeof(vfs_waiter_thread_t));
        w->thread = current_cpu->current_thread;
        w->mutex = mutex_create("waiter mutex");
        atomic_init(&w->refcount, 0);
        atomic_init(&w->has_woken_up, 0);
        current_cpu->current_thread->waiter = w;
    }
    
    mutex_acquire(current_cpu->current_thread->waiter->mutex);

    vfs_waiter_t *w = kzalloc(sizeof(vfs_waiter_t));
    w->events = events;
    w->thr = current_cpu->current_thread->waiter;
    list_append(node->waiting_nodes, w);

    atomic_fetch_add_explicit(&w->thr->refcount, 1, memory_order_relaxed);

    mutex_release(current_cpu->current_thread->waiter->mutex);

    spinlock_release(&node->waiter_lock);
    return 0;
}

/**
 * @brief mmap() a file. This is done either via the VFS' internal method or the file's
 * @param file The file to map
 * @param addr The address that was allocated for mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 * @returns Error code
 */
int fs_mmap(fs_node_t *node, void *addr, size_t size, off_t off) {
    if (!node) return -EINVAL;

    // Check if node wants to use its custom mmap
    int r = 0;
    if (node->mmap) {
        r = node->mmap(node, addr, size, off);
        goto _return;
    }

    // Load the file in at the address
    // TODO: False reading!!!! SAVE MEMORY!!
    // !!!: NOTE FOR FUTURE: If you do this you will have to also patch VAS_FAULT to NOT zero the pages
    ssize_t actual_size = fs_read(node, off, size, addr);

    // File is loaded, we're done here.    
_return:
    if (!r) {
        // Create mmap context
        if (!node->mmap_contexts) node->mmap_contexts = list_create("fs mmap contexts");
    
        vfs_mmap_context_t *ctx = kmalloc(sizeof(vfs_mmap_context_t));
        ctx->proc = current_cpu->current_process;
        ctx->addr = addr;
        ctx->size = size;
        ctx->off = off;

        list_append(node->mmap_contexts, (void*)ctx);
    }

    return r;
}

/**
 * @brief msync() a file
 * @param file The file to sync
 * @param addr The address that was allocated by mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 */
int fs_msync(fs_node_t *node, void *addr, size_t size, off_t off) {
    if (!node) return -EINVAL;

    // Check if the node wants to use its custom msync
    if (node->msync) {
        return node->msync(node, addr, size, off);
    }

    // Else, write the content in chunks (carefully avoiding potentially unallocated chunks)
    for (uintptr_t i = (uintptr_t)addr; i < (uintptr_t)addr + size; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_DEFAULT);
        if (pg) {
            if (!pg->bits.present) continue;
            
            // Has the page been touched?
            if (PAGE_IS_DIRTY(pg)) {
                // Yes, write it to the file
                // Depending on whether this iteration is the last write as much
                uintptr_t sz = ((i - (uintptr_t)(addr + size)));
                if (sz > PAGE_SIZE) sz = PAGE_SIZE;
                fs_write(node, off+(i-(uintptr_t)addr), sz, (uint8_t*)i);
            }
        }
    }

    return 0;
}

/**
 * @brief munmap a file
 * @param file The file to munmap()
 * @param addr The address that was allocated by mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 */
int fs_munmap(fs_node_t *node, void *addr, size_t size, off_t off) {
    if (!node) return -EINVAL;

    // Find the mmap context matching this node
    node_t *ctx = NULL;
    foreach(mmap_ctx_node, node->mmap_contexts) {
        vfs_mmap_context_t *mmap_ctx = (vfs_mmap_context_t*)mmap_ctx_node->value;
        if (mmap_ctx && mmap_ctx->addr == addr && mmap_ctx->size == size && mmap_ctx->off == off) {
            ctx = mmap_ctx_node;
            break;
        }
    }

    if (!ctx) {
        LOG(WARN, "Corrupt node? Could not find a valid mmap context for node \"%s\" in fs_munmap.\n", node->name);
    } else {
        list_delete(node->mmap_contexts, ctx);
    }

    // Check if the node wants to use its custom munmap
    if (node->munmap) {
        return node->munmap(node, addr, size, off);
    }

    // Else, just do an msync on the file
    return fs_msync(node, addr, size, off);
}

/**
 * @brief Truncate a file
 * @param node The node to truncate
 * @param length The new length of the file
 * @returns Error code
 */
int fs_truncate(fs_node_t *node, size_t length) {
    if (!node) return -EINVAL;

    if (node->truncate) {
        return node->truncate(node, length);
    }

    return -ENOTSUP;
}

/**
 * @brief Destroy a filesystem node immediately
 * @param node The node to destroy
 * @warning This does not check if the node has references, just use @c fs_close if you don't know what you're doing
 */
void fs_destroy(fs_node_t *node) {
    if (!node) return;

    if (node->mmap_contexts) {
        foreach(mmap_ctx_node, node->mmap_contexts) {
            vfs_mmap_context_t *ctx = (vfs_mmap_context_t*)mmap_ctx_node->value;

            // Is this part of a process?
            if (ctx->proc) {    
                foreach(map_node, ctx->proc->mmap) {
                    process_mapping_t *map = (process_mapping_t*)(map_node->value);
                    if (RANGE_IN_RANGE((uintptr_t)ctx->addr, (uintptr_t)ctx->addr+ctx->size, (uintptr_t)map->addr, (uintptr_t)map->addr + map->size)) {
                        // TODO: "Close enough" system? 
                        if (process_removeMapping(ctx->proc, map)) {
                            LOG(ERR, "Failed to remove mapping of file from %p - %p (off: %d)\n", ctx->addr, ctx->addr + ctx->size, ctx->off);
                        }
                    }
                }
            } else {
                if (fs_munmap(node, ctx->addr, ctx->size, ctx->off)) {
                    LOG(ERR, "Failed to remove mapping of file from %p - %p (off: %d)\n", ctx->addr, ctx->addr + ctx->size, ctx->off);
                }
            }

            kfree(ctx);
        }

        list_destroy(node->mmap_contexts, false);
    }

    if (node->waiting_nodes) {
        list_destroy(node->waiting_nodes, true);
    }

    kfree(node);
}

/**
 * @brief Create a copy of a filesystem node object
 * @param node The node to copy
 * @returns A new node object
 */
fs_node_t *fs_copy(fs_node_t *node) {
    // Increase refcount
    node->refcount++;
    return node;
}

/**
 * @brief Create and return a filesystem object
 * @note This API is relatively new and may not be in use everywhere
 * 
 * Does not initialize the refcount of the node. Open it somewhere,.
 */
fs_node_t *fs_node() {
    fs_node_t *node = kzalloc(sizeof(fs_node_t));
    return node;
}

/**
 * @brief Make directory
 * @param path The path of the directory
 * @param mode The mode of the directory created
 * @returns Error code
 */
int vfs_mkdir(char *path, mode_t mode) {
    // First, canonicalize the path
    char *path_canon = vfs_canonicalizePath(current_cpu->current_process->wd_path, path);
    if (!path_canon) {
        return -EINVAL;
    }

    // Now go back (UGLY)
    size_t canon_len = strlen(path_canon);
    char *parent_uncanon = kmalloc(canon_len + 4);
    snprintf(parent_uncanon, canon_len+4, "%s/..", path_canon);
    char *parent = vfs_canonicalizePath(NULL, parent_uncanon);
    kfree(parent_uncanon);
    
    // Before we free path_canon, parse it to find the last
    char *p = path_canon + (canon_len-1);
    while (p > path_canon) {
        if (*p == '/') {
            // We found a slash, exit.
            p++;
            break;
        }
        p--;
    }

    // Continue parsing past that if needed
    while (*p == '/') p++;
    
    LOG(DEBUG, "Making %s in %s\n", p, parent);

    // We should have the directory path in p
    fs_node_t *parent_node = kopen(parent, 0);
    if (!parent_node) {
        kfree(path_canon);
        kfree(parent);
        return -ENOENT;
    }

    // Does it exist?
    fs_node_t *exist_check = kopen(path_canon, 0);
    if (exist_check) {
        kfree(path_canon);
        kfree(parent);
        fs_close(parent_node);
        fs_close(exist_check);
        return -EEXIST;
    }

    // Create the directory
    int ret = -EROFS;
    if (parent_node->mkdir) {
        ret = parent_node->mkdir(parent_node, p, mode);
    }

    // Free memory
    kfree(path_canon);
    kfree(parent);
    fs_close(parent_node);
    return ret;
}

/**
 * @brief Unlink file
 * @param name The name of the file to unlink
 * @returns Error code
 */
int vfs_unlink(char *name) { return -ENOTSUP; }


/**
 * @brief creat() equivalent for VFS
 * @param node The node to output to
 * @param path The path to create
 * @param mode The mode to create with
 * @returns Error code
 * 
 * @warning This logic would much better fit into the @c kopen function with @c O_CREAT
 * @warning Some more details of this are garbage, such as errno returning with @c fs_create
 */
int vfs_creat(fs_node_t **node, char *path, mode_t mode) {
    // Make sure the path doesn't end in a /
    if (*(path + strlen(path) - 1) == '/') {
        LOG(WARN, "vfs_creat() called with path \"%s\". Directories are not accepted, please use mkdir()\n");
        return -EINVAL;
    }

    // First we have to canonicalize the path using the current process' working directory
    char *path_full = path;
    if (current_cpu->current_process) {
        path_full = vfs_canonicalizePath(current_cpu->current_process->wd_path, path);
    }

    // Now we should calculate the end.
    char *last_slash = strrchr(path_full, '/');
    if (!last_slash) {
        LOG(ERR, "last_slash not found\n");
        kfree(path_full);
        return -EINVAL;
    }


    // Hope this works..
    *last_slash = 0;
    char *parent_path = strdup(path_full);
    fs_node_t *parent = kopen(parent_path, 0);
    kfree(parent_path);
    if (!parent) {
        kfree(path_full);
        return -ENOENT;
    }

    *last_slash = '/';
    last_slash++;

    // LOG(DEBUG, "Creating file %s (file: %s)\n", path, last_slash);

    // Make sure this is a directory
    if (parent->flags != VFS_DIRECTORY) {
        kfree(path_full);
        fs_close(parent);
        return -ENOTDIR;
    }

    // Make sure the file doesn't already exist
    fs_node_t *node_test = fs_finddir(parent, last_slash);
    if (node_test) {
        fs_close(node_test);
        kfree(path_full);
        fs_close(parent);
        return -EEXIST;
    }

    // Check to see if we can create the file
    if (parent->create) {
        int error = parent->create(parent, last_slash, mode, node);
        kfree(path_full);
        fs_close(parent);
        return error;
    }

    kfree(path_full);
    fs_close(parent);
    return -EINVAL;
}

/**** VFS TREE FUNCTIONS ****/

/**
 * @brief Dump VFS tree system (recur)
 * @param node The current tree node to search
 * @param depth The depth of the tree node
 */
static void vfs_dumpRecursive(tree_node_t *node, int depth) {
    if (!node) return;

    // Calculate spaces
    char spaces[256] = { 0 };
    for (int i = 0; i < ((depth > 256) ? 256 : depth); i++) {
        spaces[i] = ' ';
    }
    
    if (node->value) {
        vfs_tree_node_t *tnode = (vfs_tree_node_t*)node->value;
        if (tnode->node) {
            LOG(DEBUG, "%s%s (filesystem %s, %p) -> file %s (%p)\n", spaces, tnode->name, tnode->fs_type, tnode, tnode->node->name, tnode->node);
        } else {
            LOG(DEBUG, "%s%s (filesystem %s, %p) -> NULL\n", spaces, tnode->name, tnode->fs_type, tnode);
        }
    } else {
        LOG(DEBUG, "%s(node %p has NULL value)\n", spaces, node);
    }

    foreach (child, node->children) {
        vfs_dumpRecursive((tree_node_t*)child->value, depth +  1);
    }

} 

/**
 * @brief Dump VFS tree system
 */
void vfs_dump() {
    LOG(DEBUG, "VFS tree dump:\n");
    vfs_dumpRecursive(vfs_tree->root, 0);
}

/**
 * @brief Initialize the virtual filesystem with no root node.
 */
void vfs_init() {
    // Create the tree
    vfs_tree = tree_create("VFS");

    // Now create a blank root node.
    vfs_tree_node_t *root_node = kmalloc(sizeof(vfs_tree_node_t));
    root_node->fs_type = strdup("N/A");
    root_node->name = strdup("/");
    root_node->node = 0;
    tree_set_parent(vfs_tree, root_node);
    
    // Create the filesystem hashmap
    vfs_filesystems = hashmap_create("VFS filesystems", 10);

    // Load spinlocks
    vfs_lock = spinlock_create("vfs lock");

    LOG(INFO, "VFS initialized\n");
}


/**
 * @brief Canonicalize a path based off a CWD and an addition.
 * 
 * This basically will turn /home/blah (CWD) + ../other_directory/gk (addition) into
 * /home/other_directory/gk
 * 
 * @param cwd The current working directory
 * @param addition What to add to the current working directory
 */
char *vfs_canonicalizePath(char *cwd, char *addition) {
    char *canonicalize_path;

    // Is the first character of addition a slash? If so, that means the path we want
    // to canonicalize is just addition.
    if (*addition == '/') {
        canonicalize_path = strdup(addition);
        goto _canonicalize;
    }

    // We can just combine the paths now.
    if (cwd[strlen(cwd)-1] == '/') {
        // cwd ends in a slash (note that normally this shouldn't happen)
        canonicalize_path = kmalloc(strlen(cwd) + strlen(addition));
        sprintf(canonicalize_path, "%s%s", cwd, addition); // !!!: unsafe
    } else {
        // cwd does not end in a slash
        canonicalize_path = kmalloc(strlen(cwd) + strlen(addition) + 1);
        sprintf(canonicalize_path, "%s/%s", cwd, addition); // !!!: unsafe
    }


_canonicalize: ;
    // At this point canonicalize_path holds a raw path to parse.
    // Something like: /home/blah/../other_directory/gk
    // We'll pull a trick from old coding and parse it into a list, iterate each element and go.
    list_t *list = list_create("canonicalize list");

    // Now add all the elements to the list, parsing each one.  
    size_t path_size = 0;  
    char *pch;
    char *save;
    pch = strtok_r(canonicalize_path, "/", &save);
    while (pch) {
        if (!strcmp(pch, "..")) {
            // pch = "..", go up one.
            node_t *node = list_pop(list);
            if (node) {
                path_size -= strlen((const char *)node->value);
                kfree(node);
                kfree(node->value);
            }
        } else if (!strcmp(pch, ".")) {
            // Don't add it to the list, it's just a "."
        } else if (*pch) {
            // Normal path, add to list
            list_append(list, strdup(pch));
            path_size += strlen(pch) + 1; // +1 for the /
        }

        pch = strtok_r(NULL, "/", &save);
    }

    // Now we can free the previous path.
    kfree(canonicalize_path);

    // Allocate the new output path (add 1 for the extra \0)
    char *output = kmalloc(path_size + 1); // Return value
    memset(output, 0, path_size + 1);

    if (!path_size) {
        // The list was empty? No '/'s? Assume root directory
        LOG(WARN, "Empty path_size after canonicalization - assuming root directory.\n");
        kfree(output); // realloc is boring 
        output = strdup("/");
    } else {
    // Append each element together
        foreach(i, list) {
            // !!!: unsafe call to sprintf
            sprintf(output, "%s/%s", output, i->value);
        }

        if (!strlen(output)) {
            *output = '/';
        }
    }

    list_destroy(list, true); // Destroy the list & free contents
    return output;
}

/**
 * @brief False VFS node read method
 */
struct dirent *vfs_fakeNodeReaddir(fs_node_t *node, unsigned long index) {
    // Of course, we gotta have the '.' and '..'
    if (index < 2) {
        struct dirent *dent = kmalloc(sizeof(struct dirent));
        memset(dent, 0, sizeof(struct dirent));
        strcpy(dent->d_name, (index == 0) ? "." : "..");
        dent->d_ino = index;
        return dent;
    }

    index-=2;

    // TODO: gross
    unsigned long i = 0;
    foreach(childnode, ((tree_node_t*)node->dev)->children) {
        if (i == index) {
            vfs_tree_node_t *vfs_node = (vfs_tree_node_t*)((tree_node_t*)childnode->value)->value;
            
            struct dirent *dent = kmalloc(sizeof(struct dirent));
            memset(dent, 0, sizeof(struct dirent));
            strncpy(dent->d_name, vfs_node->name, 256);
            dent->d_ino = i;
            return dent;
        }
        i++;
    }

    return NULL;
}

/**
 * @brief False VFS node finddir method
 */
fs_node_t *vfs_fakeNodeFinddir(fs_node_t *node, char *name) {
    foreach(childnode, ((tree_node_t*)node->dev)->children) {
        vfs_tree_node_t *vfs_node = (vfs_tree_node_t*)((tree_node_t*)childnode->value)->value;
        if (!strcmp(name, vfs_node->name)) {
            return vfs_node->node;
        }
    }

    return NULL;
}

/**
 * @brief Make a false VFS node.
 * 
 * This node is fake and allows for a simple readdir to be done which displays its tree contents.
 * The VFS tree is, well, just a collection of mountpoints - if a user tries to cd into a tree node that's just there (like /device/)
 * and doesn't actually have a filesystem then we're in trouble
 * 
 * @param name The name to use
 * @param tnode The tree node at which we should display contents
 */
fs_node_t *vfs_createFakeNode(char *name, tree_node_t *tnode) {
    fs_node_t *fakenode = kmalloc(sizeof(fs_node_t));
    memset(fakenode, 0, sizeof(fs_node_t));

    strcpy(fakenode->name, name);
    fakenode->dev = (void*)tnode;
    fakenode->flags = VFS_DIRECTORY;
    fakenode->readdir = vfs_fakeNodeReaddir;
    fakenode->finddir = vfs_fakeNodeFinddir;

    // TODO: Permissions?
    return fakenode;
}

/**
 * @brief Mount a specific node to a directory
 * @param node The node to mount
 * @param path The path to mount to
 * @returns A pointer to the tree node or NULL
 */
tree_node_t *vfs_mount(fs_node_t *node, char *path) {
    // Sanity checks
    if (!node) return NULL;
    if (!vfs_tree) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "vfs", "*** vfs_mount before init\n");
        __builtin_unreachable();
    }

    if (!path || path[0] != '/') {
        // lol
        LOG(WARN, "vfs_mount bad path argument - cannot be relative\n");
        return NULL;
    }

    tree_node_t *parent_node = vfs_tree->root; // We start at the root node

    spinlock_acquire(vfs_lock);

    // If the path strlen is 0, then we're trying to set the root node
    if (strlen(path) == 1) {
        // We don't need to allocate a new node. There's a perfectly good one already!
        vfs_tree_node_t *root = vfs_tree->root->value;
        root->node = node;
        goto _cleanup;
    }

    // Ok we still have to do work :(
    // We can use strtok_r and iterate through each part of the path, creating new nodes when needed.

    char *pch;
    char *saveptr;
    char *strtok_path = strdup(path); // strtok_r messes with the string

    pch = strtok_r(strtok_path, "/", &saveptr);
    while (pch) {
        int found = 0; // Did we find the node?

        foreach(child, parent_node->children) {
            vfs_tree_node_t *childnode = ((tree_node_t*)child->value)->value; // i hate trees
            if (!strcmp(childnode->name, pch)) {
                // Found it
                found = 1;
                parent_node = child->value;
                break;
            }
        }
    
        if (!found) {
            // LOG(INFO, "Creating node at %s\n", pch);

            vfs_tree_node_t *newnode = kmalloc(sizeof(vfs_tree_node_t)); 
            newnode->name = strdup(pch);
            newnode->fs_type = NULL;
            parent_node = tree_insert_child(vfs_tree, parent_node, newnode);
            newnode->node = vfs_createFakeNode(pch, parent_node);
        }

        pch = strtok_r(NULL, "/", &saveptr);
    } 

    // Now parent_node should point to the newly created directory
    vfs_tree_node_t *entry = parent_node->value;
    entry->node = node;

    kfree(strtok_path);

_cleanup:
    spinlock_release(vfs_lock);
    return parent_node;
}

/**
 * @brief Register a filesystem in the hashmap
 * @param name The name of the filesystem
 * @param fs_callback The callback to use
 */
int vfs_registerFilesystem(char *name, vfs_mount_callback_t mount) {
    if (!vfs_filesystems) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "vfs", "*** vfs_registerFilesystem before init\n");
        __builtin_unreachable();
    }
    
    vfs_filesystem_t *fs = kmalloc(sizeof(vfs_filesystem_t));
    fs->name = strdup(name); // no, filesystems cannot unregister themselves.
    fs->mount = mount;

    hashmap_set(vfs_filesystems, fs->name, fs);

    return 0;
}

/**
 * @brief Try to mount a specific filesystem type
 * @param name The name of the filesystem
 * @param argp The argument you wish to provide to the mount method (fs-specific)
 * @param mountpoint Where to mount the filesystem (leave as NULL if the driver takes care of this)
 * @param node Optional output for node
 * @returns Error code
 */
int vfs_mountFilesystemType(char *name, char *argp, char *mountpoint, fs_node_t **node_out) {
    if (!name) return -EINVAL;
    
    vfs_filesystem_t *fs = hashmap_get(vfs_filesystems, name);
    if (!fs) {
        LOG(WARN, "VFS tried to mount unknown filesystem type: %s\n", name);
        return -ENODEV;
    }

    if (!fs->mount) {
        LOG(WARN, "VFS found invalid filesystem '%s' when trying to mount\n", fs->name);
        return -EINVAL;
    }

    fs_node_t *node;
    int ret = fs->mount(argp, mountpoint, &node);
    if (ret) {
        return ret;
    }

    // Quick hack to allow mounting by the device itself
    if (!mountpoint) {
        if (node_out) *node_out = node;
        return 0;
    }

    tree_node_t *tnode = vfs_mount(node, mountpoint);
    if (!tnode) {
        LOG(WARN, "VFS failed to mount filesystem '%s' - freeing node\n", name);
        kfree(node);
        return -EINVAL;    
    }
    
    vfs_tree_node_t *vfsnode = (vfs_tree_node_t*)tnode->value;

    // Copy fs_type
    // TODO: Copy filesystem pointer? We should probably redo this entire system
    vfsnode->fs_type = strdup(name);

    // All done
    if (node_out) *node_out = node;
    return 0;
}

/**
 * @brief Get the mountpoint of a specific node
 * 
 * The VFS tree does not contain files part of an actual filesystem.
 * Rather it's just a collection of mountpoints.
 * 
 * Files/directories that are present on the root partition do not exist within
 * our tree - instead, finddir() is used to get them (by talking to the fs driver)
 * 
 * Therefore, the first thing that needs to be done is to get the mountpoint of a specific node.
 * 
 * @param path The path to get the mountpoint of
 * @param remainder An output of the remaining path left to search
 * @returns A duplicate of the mountpoint or NULL if it could not be found.
 */
static fs_node_t *vfs_getMountpoint(const char *path, char **remainder) {
    // Last node in the tree
    tree_node_t *last_node = vfs_tree->root;
    
    // We're going to tokenize the path, and find each token.
    char *pch;
    char *save;
    char *path_clone = strdup(path); // strtok_r trashes path

    int _remainder_depth = 0;

    pch = strtok_r(path_clone, "/", &save);
    while (pch) {
        // We have to search until we don't find a match in the tree.
        int node_found = 0; // If still 0 after foreach then we found the mountpoint.

        foreach(childnode, last_node->children) {
            tree_node_t *child = (tree_node_t*)childnode->value;
            vfs_tree_node_t *vnode = (vfs_tree_node_t*)child->value;

            if (!strcmp(vnode->name, pch)) {
                // Match found, update variables
                last_node = child;
                node_found = 1;
                break;
            }
        }

        if (!node_found) {
            break; // We found our last node.
        }
    
        _remainder_depth += strlen(pch) + 1; // + 1 for the /

        pch = strtok_r(NULL, "/", &save);
    }

    *remainder = (char*)path + _remainder_depth;
    vfs_tree_node_t *vnode = (vfs_tree_node_t*)last_node->value;
    kfree(path_clone);

    // Clone the node and return it
    fs_node_t *rnode = kmalloc(sizeof(fs_node_t));
    memcpy(rnode, vnode->node, sizeof(fs_node_t));
    rnode->refcount = 1;
    return rnode;
}



/**
 * @brief Kernel open method but relative
 * 
 * This is just an internal method used by @c kopen - it will take in the next part of the path,
 * and find the next node.
 * 
 * @todo    This needs some symlink support but that sounds like hell to implement.
 *          The only purpose of this function is to handle symlinks (and be partially recursive in doing so)
 * 
 * @param current_node The parent node to search
 * @param next_token Next token/part of the path
 * @param flags The opening flags of the file
 * @param depth Symlink depth
 * @returns A pointer to the next node, ready to be fed back into this function. NULL is returned on a failure.
 */
static fs_node_t *kopen_relative(fs_node_t *current_node, char *path, unsigned int flags, unsigned int depth) {
    if (!path || !current_node) {
        LOG(WARN, "Bad arguments to kopen_recur\n");
        return NULL;
    }

    fs_node_t *node = fs_finddir(current_node, path);
    
    if (node) {
        fs_open(node, 0);
    }

    // Close the previous node
    // fs_close(current_node);

    return node;
}


/**
 * @brief Kernel open method 
 * @param path The path of the file to open (always absolute in kmode)
 * @param flags The opening flags - see @c fcntl.h
 * 
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen(const char *path, unsigned int flags) {
    if (!path) return NULL;
    
    // !!!: TEMPORARY
    char *p = strdup(path);

    // First get the mountpoint of path.
    char *path_offset = (char*)p;
    fs_node_t *node = vfs_getMountpoint(p, &path_offset);

    if (!node) {
        kfree(p);
        return NULL; // No mountpoint
    }
    
    if (!(*path_offset)) {
        // Usually this means the user got what they want, the mountpoint, so I guess just open that and call it a da.
        goto _finish_node;
    }

    // Now we can enter a kopen_relative loop
    char *pch;
    char *save;
    pch = strtok_r(path_offset, "/", &save);

    while (pch) {
        if (!node) break;
        node = kopen_relative(node, pch, flags, 0);

        if (node && node->flags == VFS_FILE) {
            // TODO: What if the user has a REALLY weird filesystem?
            break;
        }
        
        pch = strtok_r(NULL, "/", &save); 
    }

    if (node == NULL) {
        // Not found
        kfree(p);
        return NULL;
    }

_finish_node: ;
    // Open the node
    fs_open(node, flags);
    kfree(p);

    return node;
}

/**
 * @brief Kernel open method for usermode (uses current process' working directory)
 * @param path The path of the file to open
 * @param flags The opening flags - see @c fcntl.h
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen_user(const char *path, unsigned int flags) {
    if (!path) return NULL;
    if (!current_cpu->current_process) {
        LOG(ERR, "kopen_user with no current process\n");
        return NULL;
    }

    // Canonicalize
    char *canonicalized = vfs_canonicalizePath(current_cpu->current_process->wd_path, (char*)path);
    fs_node_t *node = kopen(canonicalized, flags);
    kfree(canonicalized);
    return node;
}
