/**
 * @file hexahedron/fs/kernelfs.c
 * @brief Kernel filesystem (/kernel/) handler
 * 
 * Hexahedron uses /kernel/ to store information in a /proc/-like way
 * /kernel/processes/<pid> will store process information
 * /kernel/cpus/<cpu ID> will store CPU information
 * 
 * More nodes can be added using the KernelFS system.
 * Create a directory with @c kernelfs_createDirectory
 * Append entries to it with @c kernelfs_createEntry (using their callbacks)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/kernelfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Parent node */
kernelfs_dir_t *kernelfs_parent = NULL;

/* Process list */
extern list_t *process_list;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:KERNELFS", __VA_ARGS__)


/**
 * @brief /kernel/processes/<pid>/XXX read
 */
ssize_t kernelfs_processdirRead(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    char tmp_buffer[512];
    node->length = snprintf((char*)tmp_buffer, 512,
            "ProcessName:%s\n",
                ((process_t*)node->dev)->name);

    if (!size) return 0;

    if (off > (off_t)node->length) return 0;
    if (off + size > node->length) size = (off_t)node->length - off; 

    LOG(DEBUG, "Processes read() copy %s to buffer %p sz %d\n",tmp_buffer, buffer, size);
    // if (buffer) strncpy((char*)buffer, tmp_buffer, min(node->length, size));
    if (buffer) strcpy((char*)buffer, "balls");
    return size;

}

/**
 * @brief /kernel/processes/<pid>/XXX open
 */
void kernelfs_processdirOpen(fs_node_t *node, unsigned int flags) {
    fs_read(node, 0, 0, NULL);
}

/**
 * @brief /kernel/processes/<pid> finddir method
 */
fs_node_t *kernelfs_processdirFinddir(fs_node_t *node, char *path) {
    if (!strncmp(path, "info", 256)) {
        fs_node_t *file = kmalloc(sizeof(fs_node_t));
        memset(file, 0, sizeof(fs_node_t));
        snprintf(file->name, 256, "%s", path);
        file->flags = VFS_FILE;
        file->atime = file->mtime = file->ctime = now();
        file->mask = 0777;
        file->open = kernelfs_processdirOpen;
        file->read = kernelfs_processdirRead;
        file->dev = (void*)node->dev;
        return file;
    }

    return NULL;
}


/**
 * @brief /kernel/processes/<pid> readdir method
 */
struct dirent *kernelfs_processdirReaddir(fs_node_t *node, unsigned long index) {
    // Handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    if (index == 0) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, "info");
        out->d_ino = 0;
        return out;
    }

    return NULL;
}

/**
 * @brief /kernel/processes finddir method
 */
fs_node_t *kernelfs_processesFinddir(fs_node_t *node, char *path) {
    // TODO: better
    int pid = atoi(path);

    foreach(proc_node, process_list) {
        process_t *proc = ((process_t*)proc_node->value);
        if (!proc) continue;
        if ((int)proc->pid == pid) {
            fs_node_t *file = kmalloc(sizeof(fs_node_t));
            memset(file, 0, sizeof(fs_node_t));
            snprintf(file->name, 256, "%i", proc->pid);
            file->flags = VFS_DIRECTORY;
            file->atime = file->mtime = file->ctime = now();
            file->mask = 0777;
            file->readdir = kernelfs_processdirReaddir;
            file->finddir = kernelfs_processdirFinddir;
            file->dev = (void*)proc;
            return file;
        }
    }

    return NULL;
}

/**
 * @brief /kernel/processes readdir method
 */
struct dirent *kernelfs_processesReaddir(fs_node_t *node, unsigned long index) {
    // Handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    // For each process put a new directory entry
    unsigned long i = 0;
    foreach(proc_node, process_list) {
        process_t *proc = ((process_t*)proc_node->value);
        if (!proc) continue;
        if (i == index) {
            // This is the node we want
            struct dirent *out = kmalloc(sizeof(struct dirent));
            snprintf(out->d_name, 512, "%i", proc->pid);
            out->d_ino = proc->pid;
            return out;
        }

        i++;
    }

    return NULL;
}

/**
 * @brief Default KernelFS read directory method
 * 
 * This will check @c entries and return @c dirent objects for each entry.
 */
static struct dirent *kernelfs_genericReaddir(fs_node_t *node, unsigned long index) {
    // Handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    // If we're not doing children, what are we doing?
    kernelfs_dir_t *dir = (kernelfs_dir_t*)node->dev;
    if (!dir->entries) {
        LOG(WARN, "Generic readdir() on a custom directory node?\n");
        return NULL;
    }

    // Start iterating
    unsigned long i = 0;
    foreach(entry_node, dir->entries) {
        if (i != index) {
            i++;
            continue;
        } 
        
        kernelfs_dir_t *dir = entry_node->value;
        if (!dir) continue;
        
        // Check type
        if (dir->type == KERNELFS_ENTRY) {
            // Entry
            kernelfs_entry_t *entry = (kernelfs_entry_t*)entry_node->value;
            struct dirent *out = kmalloc(sizeof(struct dirent));
            strcpy(out->d_name, entry->node->name);
            out->d_ino = entry->node->inode;
            return out;
        } else {    
            // Directory
            struct dirent *out = kmalloc(sizeof(struct dirent));
            strcpy(out->d_name, dir->node->name);
            out->d_ino = dir->node->inode;
            return out;
        }
        
        // ???
        i++;
    }

    return NULL;
}

/**
 * @brief Default KernelFS find directory method
 * 
 * This will search the directory entries for a specific path.
 */
static fs_node_t *kernelfs_genericFinddir(fs_node_t *node, char *path) {

    if (!path) return NULL;

    // If we're not doing children, what are we doing?
    kernelfs_dir_t *dir = (kernelfs_dir_t*)node->dev;
    if (!dir->entries) {
        LOG(WARN, "Generic finddir() on a custom directory node?\n");
        return NULL;
    }

    // Check children
    foreach(entry_node, dir->entries) {
        kernelfs_dir_t *dir = entry_node->value;
        if (!dir) continue;

        // Check type
        if (dir->type == KERNELFS_ENTRY) {
            // Entry
            kernelfs_entry_t *entry = (kernelfs_entry_t*)entry_node->value;
            if (strncmp(path, entry->node->name, 256)) continue;
            return entry->node;
        } else {    
            // Directory
            if (strncmp(path, dir->node->name, 256)) continue;
            return dir->node;
        }
    }

    return NULL;
}

/**
 * @brief Generic read method for the KernelFS
 */
static ssize_t kernelfs_genericRead(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    kernelfs_entry_t *entry = (kernelfs_entry_t*)node->dev;
    if (!entry->finished) {
        if (entry->get_data(entry, entry->data)) {
            LOG(ERR, "Failed to get data for node \"%s\"\n", node->name);
            return 1;
        }
    }

    if (off > (off_t)node->length) return 0;
    if (off + size > node->length) {
        size = node->length - off;
    }

    if (!size) return 0;
    if (!buffer) return 0;
    memcpy(buffer, entry->buffer + off, size);
    return size;
}

/**
 * @brief Generic open method to handle @c stat getting st_size
 * 
 * This is kinda annoying... make it stop..
 */
void kernelfs_genericOpen(fs_node_t *node, unsigned int mode) {
    // !!!: kill me, it's too late here.
    // This will update node->length
    kernelfs_genericRead(node, 0, 0, NULL);
    return;
}

/**
 * @brief Generic close method
 */
static void kernelfs_genericClose(fs_node_t *node) {
    kernelfs_entry_t *entry = (kernelfs_entry_t*)(node->dev);
    if (entry->buffer) {
        kfree(entry->buffer);
    } 
}

/**
 * @brief CB write method for @c kernelfs_writeData
 * 
 * i love xvasprintf!!!
 */
static int kernelfs_cbWrite(void *user, char ch) {
    kernelfs_entry_t *entry = (kernelfs_entry_t*)user;

    entry->buffer[entry->buflen] = ch;
    entry->buflen++;

    if (entry->buflen >= entry->bufsz) {
        entry->buffer = krealloc(entry->buffer, entry->bufsz * 2);
        entry->bufsz *= 2;
    }

    return 0;
}

/**
 * @brief Write data method for the KernelFS
 * @param entry The KernelFS entry to write data to
 * @param fmt The format string to use 
 */
int kernelfs_writeData(kernelfs_entry_t *entry, char *fmt, ...) {

    if (!entry->buffer) {
        entry->buffer = kmalloc(KERNELFS_DEFAULT_BUFFER_LENGTH);
        memset(entry->buffer, 0, KERNELFS_DEFAULT_BUFFER_LENGTH);
        entry->bufsz = KERNELFS_DEFAULT_BUFFER_LENGTH;
    }
    entry->buflen = 0;

    va_list ap;
    va_start(ap, fmt);
    int ret = xvasprintf(kernelfs_cbWrite, (void*)entry, fmt, ap);
    va_end(ap);

    // Setup node length
    entry->node->length = entry->buflen; 
    return ret;
}

/**
 * @brief Create a new directory entry for the KernelFS
 * @param parent The parental filesystem structure or NULL to mount under root
 * @param name The name of the directory entry (node->name)
 * @param use_entries 1 if you want to use the entries list. Use them unless you have no idea what you're doing.
 * @returns The directory entry
 */
kernelfs_dir_t *kernelfs_createDirectory(kernelfs_dir_t *parent, char *name, int use_entries) {
    if (!parent) parent = kernelfs_parent;

    kernelfs_dir_t *dir = kmalloc(sizeof(kernelfs_dir_t));
    dir->parent = parent;
    dir->entries = (use_entries) ? list_create("kernelfs entries") : NULL;
    dir->type = KERNELFS_DIR;

    // Start creating the node
    fs_node_t *node = kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));
    strncpy(node->name, name, 256);
    node->flags = VFS_DIRECTORY;
    node->mask = 0777;                                  // !!!: Probably bad?
    node->readdir = kernelfs_genericReaddir;
    node->finddir = kernelfs_genericFinddir;
    node->dev = (void*)dir;

    // Add to parent
    if (dir->parent && dir->parent->entries) {
        list_append(dir->parent->entries, (void*)dir);
    }

    // Setup node and return
    dir->node = node;
    return dir;
}


/**
 * @brief Create a new entry under a directory for the KernelFS
 * @param dir The directory under which to add a new entry (or NULL for root)
 * @param name The name of the directory entry
 * @param get_data The get data function of which to use
 * @param data User-provided data
 */
kernelfs_entry_t *kernelfs_createEntry(kernelfs_dir_t *dir, char *name, kernelfs_get_data_t get_data, void *data) {
    if (!dir) dir = kernelfs_parent;

    kernelfs_entry_t *entry = kmalloc(sizeof(fs_node_t));
    memset(entry, 0, sizeof(kernelfs_entry_t));
    entry->type = KERNELFS_ENTRY;
    entry->get_data = get_data;
    entry->data = data;

    fs_node_t *node = kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));
    strncpy(node->name, name, 256);
    node->flags = VFS_FILE;
    node->open = kernelfs_genericOpen;
    node->close = kernelfs_genericClose;
    node->read = kernelfs_genericRead;
    
    node->mask = 0777;
    node->ctime = node->atime = node->mtime = now();
    node->dev = (void*)entry;

    // Add to parent
    if (dir && dir->entries) {
        list_append(dir->entries, (void*)entry);
    }

    entry->node = node;
    return entry;
}

/**
 * @brief Initialize the kernel filesystem
 */
void kernelfs_init() {
    // Create parental node
    kernelfs_parent = kernelfs_createDirectory(NULL, "kernel", 1);
    vfs_mount(kernelfs_parent->node, "/kernel");

    // Create processes
    kernelfs_dir_t *proc = kernelfs_createDirectory(NULL, "processes", 0);
    proc->node->readdir = kernelfs_processesReaddir;
    proc->node->finddir = kernelfs_processesFinddir;
}