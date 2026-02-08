/**
 * @file hexahedron/include/kernel/fs/systemfs.h
 * @brief SystemFS
 * 
 * SystemFS is the successor to KernelFS, which was the legacy kernel filesystem.
 * It provides a more robust API, similar to that of DevFS.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_SYSTEMFS_H
#define KERNEL_FS_SYSTEMFS_H

/**** INCLUDES ****/
#include <kernel/fs/vfs_new.h>
#include <structs/hashmap.h>
#include <stdint.h>


/**** DEFINITIONS ****/


/**** TYPES ****/

struct systemfs_node;

struct systemfs_print_ctx {
    char *buffer;
    size_t size;
    loff_t off;
    int idx;
};

typedef struct systemfs_ops {
    int (*open)(struct systemfs_node *file, unsigned long flags);
    int (*close)(struct systemfs_node *file);
    ssize_t (*read)(struct systemfs_node *file, loff_t off, size_t size, char *buffer);
    ssize_t (*write)(struct systemfs_node *file, loff_t off, size_t size, const char *buffer);
    int (*ioctl)(struct systemfs_node *file, unsigned long request, void *argp);
    int (*read_entry)(struct systemfs_node *node, vfs_dir_context_t *ctx); // same pattern as get_entries in VFS
    int (*lookup)(struct systemfs_node *node, char *name, struct systemfs_node **node_out);
    int (*lseek)(struct systemfs_node *node, loff_t off, int whence, loff_t *pos); // this is REQUIRED if not using simple
    ssize_t (*read_simple)(struct systemfs_node *node); // hack
} systemfs_ops_t;

typedef struct systemfs_buffer {
    char *buffer;
    size_t bufidx;
    size_t bufsize;
} systemfs_buffer_t;

typedef struct systemfs_node {
    struct systemfs_node *parent;       // Parent
    char *name;                         // Name of the node
    systemfs_ops_t *ops;                // SystemFS operations
    hashmap_t *children;                // Children
    void *priv;                         // Private
    vfs_inode_attr_t attr;              // Attribute
    mutex_t lck;                        // Lock
    char *link_contents;                // Link contents
    systemfs_buffer_t buf;              // SystemFS buffer
} systemfs_node_t;

/**** MACROS ****/

#define SYSTEMFS_PRINT_CTX_INIT(ctx, buf, offset, sz) { (ctx)->idx = 0; (ctx)->buffer = (buf); (ctx)->off = (offset); (ctx)->size = (sz); }
#define SYSTEMFS_PRINT_CTX_NOT_DONE(ctx) ((size_t)((ctx)->idx - (ctx)->off) < (ctx)->size)

/**** FUNCTIONS ****/

extern systemfs_node_t *systemfs_root;

/**
 * @brief Register a new SystemFS node
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 * @param type The type of the SystemFS node (VFS_xxx)
 * @param ops The operations of the SystemFS node
 * @param priv Private variables for the SystemFS node
 * @returns New node
 */
systemfs_node_t *systemfs_register(systemfs_node_t *parent, char *name, int type, systemfs_ops_t *ops, void *priv);

/**
 * @brief Unregister SystemFS entry
 * @param parent The parent of the SystemFS entry to unregister
 * @param name The name of the SystemFS entry to unregister
 */
int systemfs_unregister(systemfs_node_t *parent, char *name);

/**
 * @brief Get SystemFS node
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 */
systemfs_node_t *systemfs_get(systemfs_node_t *parent, char *name);

/**
 * @brief Register a "simple" SystemFS node
 * 
 * Simple SystemFS nodes are nodes that only provide either reading/writing.
 * The "read" method is called not just on read but also on opening to fill the SystemFS buffer.
 * Write is only called on writes.
 *
 * @warning A simple node will NOT be updatable after it has been opened.
 * 
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 * @param read Reading function for the SystemFS node
 * @param write Writing function for the SystemFS node
 * @param priv Private variable
 * @returns The node registered
 */
systemfs_node_t *systemfs_registerSimple(systemfs_node_t *parent, char *name, ssize_t (*read)(systemfs_node_t *), ssize_t (*write)(systemfs_node_t *, loff_t, size_t, const char*), void *priv);

/**
 * @brief Create a link in SystemFS
 * @param parent The parent of the link
 * @param link_name The link name
 * @param link_contents The link contents
 */
systemfs_node_t *systemfs_symlink(systemfs_node_t *parent, char *link_name, char *link_contents);

/**
 * @brief Create and return a SystemFS directory
 * @param parent The parent of the directory
 * @param name The name of the directory
 * @note You can also do this with @c systemfs_register by specifying VFS_DIRECTORY as the type.
 */
systemfs_node_t *systemfs_createDirectory(systemfs_node_t *parent, char *name);

/**
 * @brief Printf-formatter for SystemFS
 * @param node The node to print out to
 * @param fmt Format
 */
ssize_t systemfs_printf(systemfs_node_t *node, char *fmt, ...);

#endif