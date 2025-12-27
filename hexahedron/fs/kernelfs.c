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
#include <kernel/mm/alloc.h>
#include <kernel/task/process.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <kernel/misc/util.h>
#include <structs/list.h>
#include <kernel/drivers/clock.h>
#include <structs/hashmap.h>
#include <kernel/config.h>
#include <kernel/init.h>
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
    process_t *proc = (process_t*)node->dev;

    // Copy to buffer
    char tmp_buffer[512] = { 0 };
    
    switch (node->inode) {
        case 1:
            node->length = snprintf((char*)tmp_buffer, 512,
                    "ProcessName:%s\n"
                    "ProcessPid:%d\n"
                    "Uid:%d\n"
                    "Gid:%d\n"
                    "Euid:%d\n"
                    "Egid:%d\n"
                    "Sid:%d\n"
                    "Pgid:%d\n"
                    "KernelStack:%p\n"
                    "Parent:%s\n",
                        proc->name,
                        proc->pid,
                        proc->uid, proc->gid, proc->euid, proc->egid,
                        proc->sid, proc->pgid,
                        proc->kstack,
                        (proc->parent ? proc->parent->name : "N/A"));
        
            break;

        case 2:
            node->length = 0;
            for (size_t i = 0; i < proc->fd_table->total; i++) {
                fd_t *fd = proc->fd_table->fds[i];
                if (fd) {
                    char tmp_buffer2[512] = { 0 };
                    node->length += snprintf(tmp_buffer2, 512-strlen(tmp_buffer),
                        "FileDescriptor:%d\n"
                        "Name:%s\n",
                            fd->fd_number,
                            fd->node->name);
                
                    strcat(tmp_buffer, tmp_buffer2);
                }
            }
            
            break;

        default:
            node->length = 0;
    }


    // Calculate off and size
    if (!size) return 0;
    if (off > (off_t)node->length) return 0;
    if (off + size > node->length) size = (off_t)node->length - off; 

    // Copy and return
    if (buffer) strncpy((char*)buffer, tmp_buffer, min(node->length, size));
    return size;
}

/**
 * @brief /kernel/processes/<pid>/XXX open
 */
int kernelfs_processdirOpen(fs_node_t *node, unsigned int flags) {
    return fs_read(node, 0, 0, NULL);
}

/**
 * @brief /kernel/processes/<pid> finddir method
 */
fs_node_t *kernelfs_processdirFinddir(fs_node_t *node, char *path) {
    fs_node_t *file = kmalloc(sizeof(fs_node_t));
    memset(file, 0, sizeof(fs_node_t));
    snprintf(file->name, 256, "%s", path);
    file->flags = VFS_FILE;
    file->atime = file->mtime = file->ctime = now();
    file->mask = 0777;
    file->open = kernelfs_processdirOpen;
    file->read = kernelfs_processdirRead;
    file->dev = (void*)node->dev;

    if (!strncmp(path, "info", 256)) {
        file->inode = 1;
        return file;
    } else if (!strncmp(path, "fds", 256)) {
        file->inode = 2;
        return file;
    }

    kfree(file);
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

    // Maximum count
    if (index > 1) {
        return NULL;
    }

    // Get a dirent
    struct dirent *out = kmalloc(sizeof(struct dirent));
    out->d_ino = 0;

    if (index == 0) {
        strcpy(out->d_name, "info");
        return out;
    } else {
        strcpy(out->d_name, "fds");
        return out;
    }
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
ssize_t kernelfs_genericRead(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    kernelfs_entry_t *entry = (kernelfs_entry_t*)node->dev;
    if (!entry->finished) {
        entry->buflen = 0;
        if (entry->get_data(entry, entry->data)) {
            LOG(ERR, "Failed to get data for node \"%s\"\n", node->name);
            return 1;
        }
    }

    if (off >= (off_t)node->length) return 0;
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
int kernelfs_genericOpen(fs_node_t *node, unsigned int mode) {
    // Reset length
    kernelfs_entry_t *entry = (kernelfs_entry_t*)node->dev;
    
    // !!!: kill me, it's too late here.
    // This will update node->length
    LOG(INFO, "Opened: Updating node->length\n");
    kernelfs_genericRead(node, 0, 0, NULL);

    // !!!: Because vfs_open creates a copy of the node, update this copies length from the original
    node->length = entry->node->length;

    return 0;
}

/**
 * @brief Generic close method
 */
static int kernelfs_genericClose(fs_node_t *node) {
    // Don't free, let others reuse   
    return 0;
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
 * @brief Append data method for the KernelFS
 * @param entry The KernelFS entry to append data to
 * @param fmt The format string to use 
 */
int kernelfs_appendData(kernelfs_entry_t *entry, char *fmt, ...) {
    if (!entry->buffer) {
        entry->buffer = kmalloc(KERNELFS_DEFAULT_BUFFER_LENGTH);
        memset(entry->buffer, 0, KERNELFS_DEFAULT_BUFFER_LENGTH);
        entry->bufsz = KERNELFS_DEFAULT_BUFFER_LENGTH;
        entry->buflen = 0;
    }

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
    fs_open(node, 0);

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

    fs_node_t *node = fs_node();
    strncpy(node->name, name, 256);
    node->flags = VFS_FILE;
    node->open = kernelfs_genericOpen;
    node->close = kernelfs_genericClose;
    node->read = kernelfs_genericRead;
    
    node->mask = 0777;
    node->ctime = node->atime = node->mtime = now();
    node->dev = (void*)entry;
    node->refcount++; // fs_open would fuck itself

    // Add to parent
    if (dir && dir->entries) {
        list_append(dir->entries, (void*)entry);
    }

    entry->node = node;
    return entry;
}

/**
 * @brief Remove an entry under a directory for the KernelFS
 * @param dir The directory to remove the entry from
 * @param entry The entry to remove 
 */
int kernelfs_removeEntry(kernelfs_dir_t *dir, kernelfs_entry_t *entry) {
    if (dir->entries) list_delete(dir->entries, list_find(dir->entries, (void*)entry));
    return 0;
}

/**
 * @brief Read kernel memory information
 * @param entry The entry to read
 * @param data NULL
 */
int kernelfs_memoryRead(kernelfs_entry_t *entry, void *data) {
    uintptr_t total_blocks = pmm_getTotalBlocks();
    uintptr_t used_blocks = pmm_getUsedBlocks();
    uintptr_t free_blocks = pmm_getFreeBlocks();
    uintptr_t kernel_in_use = alloc_used();

    kernelfs_writeData(entry,
            "TotalPhysBlocks:%d\n"
            "TotalPhysMemory:%zu kB\n"
            "UsedPhysMemory:%zu kB\n"
            "FreePhysMemory:%zu kB\n"
            "KernelMemoryAllocator:%zu\n",
                total_blocks,
                total_blocks * PAGE_SIZE / 1000,
                used_blocks * PAGE_SIZE / 1000,
                free_blocks * PAGE_SIZE / 1000,
                kernel_in_use);
    return 0;
}

/**
 * @brief Read kernel version information
 * @param entry The entry to read
 * @param data NULL
 */
int kernelfs_versionRead(kernelfs_entry_t *entry, void *data) {
    kernelfs_writeData(entry,
        "KernelName:Hexahedron\n"
        "KernelVersionMajor:%d\n"
        "KernelVersionMinor:%d\n"
        "KernelVersionLower:%d\n"
        "KernelCodename:%s\n"
        "Compiler:%s\n"
        "BuildDate:%s\n"
        "BuildTime:%s\n",
            __kernel_version_major,
            __kernel_version_minor,
            __kernel_version_lower,
            __kernel_version_codename,
            __kernel_compiler,
            __kernel_build_date,
            __kernel_build_time);
    return 0;
}

/**
 * @brief Read kernel cmdline information
 * @param entry The entry to read
 * @param data NULL
 */
int kernelfs_cmdlineRead(kernelfs_entry_t *entry, void *data) {
    kernelfs_writeData(entry,
        "%s\n", arch_get_generic_parameters()->kernel_cmdline);
    return 0;
}

/**
 * @brief Read kernel uptime
 * @param entry The entry to read
 * @param data NULL
 */
int kernelfs_uptimeRead(kernelfs_entry_t *entry, void *data) {
    unsigned long seconds, subseconds;
    clock_relative(0, 0, &seconds, &subseconds);

    kernelfs_writeData(entry,
        "%lu.%016lu\n", seconds, subseconds);
    return 0;
}

/**
 * @brief Kernel filesystems read method
 * @param entry The entry to read
 * @param data NULL
 */
int kernelfs_filesystemsRead(kernelfs_entry_t *entry, void *data) {
extern hashmap_t *vfs_filesystems;
    foreach(key, hashmap_keys(vfs_filesystems)) {
        kernelfs_appendData(entry, "%s\n", key->value);
    }

    return 0;
}


/**
 * @brief Mount KernelFS on a different directory (basically just a symlink)
 */
int kernelfs_mount(char *argp, char *mountpoint, fs_node_t **node_out) {
    *node_out =  kernelfs_parent->node;
    return 0;
}

/**
 * @brief Initialize the kernel filesystem
 */
int kernelfs_init() {
    vfs_registerFilesystem("kernelfs", kernelfs_mount);

    // Create parental node
    kernelfs_parent = kernelfs_createDirectory(NULL, "kernel", 1);
    vfs_mount(kernelfs_parent->node, "/kernel");

    // Create processes
    kernelfs_dir_t *proc = kernelfs_createDirectory(NULL, "processes", 0);
    proc->node->readdir = kernelfs_processesReaddir;
    proc->node->finddir = kernelfs_processesFinddir;

    // Create memory
    kernelfs_createEntry(NULL, "memory", kernelfs_memoryRead, NULL);

    // Create version
    kernelfs_createEntry(NULL, "version", kernelfs_versionRead, NULL);

    // Create cmdline
    kernelfs_createEntry(NULL, "cmdline", kernelfs_cmdlineRead, NULL);

    // Create filesystems
    kernelfs_createEntry(NULL, "filesystems", kernelfs_filesystemsRead, NULL);

    // Create uptime
    kernelfs_createEntry(NULL, "uptime", kernelfs_uptimeRead, NULL);
    return 0;
}
    

FS_INIT_ROUTINE(kernelfs, INIT_FLAG_DEFAULT, kernelfs_init);