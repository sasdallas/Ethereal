/**
 * @file hexahedron/include/kernel/fs/kernelfs.h
 * @brief Kernel data filesystem handler (/kernel/)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_KERNELFS_H
#define KERNEL_FS_KERNELFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

#define KERNELFS_DEFAULT_BUFFER_LENGTH      256

/* Ugly types */
#define KERNELFS_ENTRY      0
#define KERNELFS_DIR        1

/**** TYPES ****/

struct kernelfs_entry;

/**
 * @brief kernelfs get data function
 * 
 * Called by kernelfs when a read is attempted on the file registered to the entry.
 * Should use @c kernelfs_writeData()
 * 
 * @param entry The kernelfs entry that was registered
 * @param data User-provided data
 * @returns 0 on success
 */
typedef int (*kernelfs_get_data_t)(struct kernelfs_entry *entry, void *data);

/**
 * @brief kernelfs file entry
 */
typedef struct kernelfs_entry {
    int type;                       // Type identifier.
                                    // !!!: I HATE THIS! This is a basic hack that lets the kernelfs driver not know what entry is which.
                                    // !!!: In the name of memory conservation (i.e. not having two lists), its fine

    fs_node_t *node;                // Filesystem node registered
    char *buffer;                   // Pointer to the buffer
    size_t buflen;                  // Length of the buffer
    size_t bufsz;                   // Size of the buffer
    int finished;                   // There is no need to call the get data function
    kernelfs_get_data_t get_data;   // Get data function 
    void *data;                     // User-provided data
} kernelfs_entry_t;

/**
 * @brief kernelfs directory entry
 */
typedef struct kernelfs_dir {
    int type;                       // Type identifier.
                                    // !!!: I HATE THIS! This is a basic hack that lets the kernelfs driver not know what entry is which.
                                    // !!!: In the name of memory conservation (i.e. not having two lists), its fine

    struct kernelfs_dir *parent;    // Parental node
    fs_node_t *node;                // Filesystem node registered
    list_t *entries;                // List of entries for the kernelfs usage.
                                    // NOTE: Disregarding this is fine. The kernel will disregard it for /kernel/processes.
                                    // NOTE: Simply set node->readdir and node->finddir yourself
} kernelfs_dir_t;


/**** FUNCTIONS ****/

/**
 * @brief Initialize the kernel filesystem
 */
void kernelfs_init();

/**
 * @brief Write data method for the KernelFS
 * @param entry The KernelFS entry to write data to
 * @param fmt The format string to use 
 */
int kernelfs_writeData(kernelfs_entry_t *entry, char *fmt, ...);

/**
 * @brief Create a new directory entry for the KernelFS
 * @param parent The parental filesystem structure or NULL to mount under root
 * @param name The name of the directory entry (node->name)
 * @param use_entries 1 if you want to use the entries list. Use them unless you have no idea what you're doing.
 * @returns The directory entry
 */
kernelfs_dir_t *kernelfs_createDirectory(kernelfs_dir_t *parent, char *name, int use_entries);

/**
 * @brief Create a new entry under a directory for the KernelFS
 * @param dir The directory under which to add a new entry (or NULL for root)
 * @param name The name of the directory entry
 * @param get_data The get data function of which to use
 * @param data User-provided data
 */
kernelfs_entry_t *kernelfs_createEntry(kernelfs_dir_t *dir, char *name, kernelfs_get_data_t get_data, void *data);

/**
 * @brief Generic read method for the KernelFS
 */
ssize_t kernelfs_genericRead(fs_node_t *node, off_t off, size_t size, uint8_t *buffer);

#endif