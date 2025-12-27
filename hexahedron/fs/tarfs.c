/**
 * @file hexahedron/fs/tarfs.c
 * @brief USTAR archive filesystem, used for the initial ramdisk
 * 
 * @copyright
 * This file is part of Ethereal Operating System, which is created by Samuel.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel S.
 * 
 * @copyright
 * Some parts of this file may be attributed to ToaruOS. It is unclear and still being worked out.
 */

#include <kernel/fs/tarfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <kernel/init.h>

/* Macro to round to 512 (for size) */
#define USTAR_SIZE(size) ((size % 512) ? (size + (512 - (size % 512))) : size)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:TARFS", __VA_ARGS__)

/* Prototypes */
ssize_t tarfs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);
ssize_t tarfs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);
fs_node_t *tarfs_finddir(fs_node_t *node, char *path);
struct dirent *tarfs_readdir(fs_node_t *node, unsigned long index);
ssize_t tarfs_readlink(struct fs_node *node, char *buf, size_t size);


/**
 * @brief Convert a file inode to a ustar header
 */
static int tarfs_getUstar(fs_node_t *node, uint64_t inode, ustar_header_t *header) {
    fs_node_t *dev = (fs_node_t*)node->dev;
    if (!node->dev) goto _no_header;

    // Read into header
    if (fs_read(dev, inode, sizeof(ustar_header_t), (uint8_t*)header) == 0) {
        // We didn't get anything..?
        goto _no_header;
    } 

    // Validate USTAR header
    if (!strncmp(header->ustar, "ustar", 5)) {
        // Valid!
        return 0;
    }

    // Not valid, free and return 1
_no_header:
    return 1;
}

/**
 * @brief Convert a ustar header into a file node
 */
static fs_node_t *tarfs_ustarToNode(ustar_header_t *header, uint64_t inode, fs_node_t *parent_node) {
    if (!header) return NULL;

    // Allocate a new node
    fs_node_t *node = fs_node();

    // Copy name
    // !!!: symlinks need linkname
    snprintf(node->name, 255, "%s%s", header->nameprefix, header->name);
    
    // Interpret the type now
    switch (header->type[0]) {
        case USTAR_HARD_LINK:
            LOG(ERR, "Cannot parse entry '%s' type (USTAR_HARD_LINK) - kernel bug\n", header->name);
            node->flags = VFS_SYMLINK;
            break;

        case USTAR_SYMLINK:
            node->flags = VFS_SYMLINK;
            break;

        case USTAR_CHARDEV:
            node->flags = VFS_CHARDEVICE;
            break;

        case USTAR_BLOCKDEV:
            node->flags = VFS_BLOCKDEVICE;
            break;

        case USTAR_DIRECTORY:
            node->flags = VFS_DIRECTORY;
            break;

        case USTAR_PIPE:
            node->flags = VFS_PIPE;
            break;
        
        default:
            node->flags = VFS_FILE;
            break;
    }

    // Interpret UID, GID, mode, and length.
    node->length = strtoull(header->size, NULL, 8); // i hate octal
    node->gid = strtol(header->gid, NULL, 8);
    node->uid = strtol(header->uid, NULL, 8);
    node->mask = strtol(header->mode, NULL, 8);

    node->inode = inode;
    node->dev = parent_node->dev;
    
    // Setup functions
    node->open = NULL;
    node->close = NULL;
    node->read = tarfs_read;
    node->write = tarfs_write;
    node->finddir = tarfs_finddir;
    node->readdir = tarfs_readdir;
    node->readlink = tarfs_readlink;

    return node;
}


/**
 * @brief tarfs readlink method
 */
ssize_t tarfs_readlink(struct fs_node *node, char *buf, size_t size) {
    // Read the ustar header for this node
    ustar_header_t header;
    assert(!tarfs_getUstar(node, node->inode, &header));
    

    // Get the node
    size_t sz_to_read = strlen(header.linkname) > size ? size : strlen(header.linkname);
    memcpy(buf, header.linkname, sz_to_read); 

    return sz_to_read;
}


/**
 * @brief tarfs read method
 */
ssize_t tarfs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if ((node->flags & VFS_FILE) == 0) return 0;
    if ((size_t)offset > node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    uint64_t read_offset = node->inode + 512 + offset;
    return fs_read((fs_node_t*)node->dev, read_offset, size, buffer);
}

/**
 * @brief tarfs write method
 */
ssize_t tarfs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if ((node->flags & VFS_FILE) == 0) return 0;
    if ((size_t)offset > node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    uint64_t write_offset = node->inode + 512 + offset;
    return fs_write((fs_node_t*)node->dev, write_offset, size, buffer);
}

/**
 * @brief tarfs readdir (root) method
 */
struct dirent *tarfs_readdir_root(fs_node_t *node, unsigned long index) {
    // First, if index == 0 or 1, return ./.. respectively
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 1; // Start from a 1-index, that way we don't return node. 

    if (!node) return NULL;

    // Get the header and verify it
    uint64_t ino = node->inode;
    ustar_header_t header;
    if (tarfs_getUstar(node, ino, &header)) {
        // Invalid ustar
        return NULL;
    }

    int fileidx = 0; // Index of the current file
    
    // Start iterating
    while (1) {
        // Construct the full filename
        char path_filename[256] = { 0 };
        strncat(path_filename, header.nameprefix, 155);
        strncat(path_filename, header.name, 150);

        int slashes = 0;
        char *s = (char*)path_filename;
        for (int idx = 0; s[idx]; (s[idx]=='/' && s[idx+1]) ? slashes++, idx++ : idx++);

        // Use strstr to find the occurence of search_filename in path_filename (or if !slashes we checked that earlier)
        if (!slashes) {
            char *slash = strstr(path_filename, "/");
            if (slash) *slash = 0;

            // Match found - is it the one we want?
            if (fileidx == (int)index && strlen(path_filename)) {
                // Found the right one!
                struct dirent *out = kmalloc(sizeof(struct dirent));
                memset(out, 0, sizeof(struct dirent));
                out->d_ino = ino;
                
                // Update name to remove last slash
                if (path_filename[strlen(path_filename) - 1] == '/') {
                    path_filename[strlen(path_filename) - 1] = '\0';
                }

                // Copy name
                strncpy(out->d_name, path_filename, 256);
    
                // Setup other parameters
                out->d_reclen = strtoull(header.size, NULL, 8);
                
                // Interpret the type now
                switch (header.type[0]) {
                    case USTAR_HARD_LINK:
                        LOG(ERR, "Cannot parse entry '%s' type (USTAR_HARD_LINK) - kernel bug\n", header.name);
                        // Fallthrough
                    case USTAR_SYMLINK:
                        out->d_type = DT_LNK;
                        break;

                    case USTAR_CHARDEV:
                        out->d_type = DT_CHR;
                        break;

                    case USTAR_BLOCKDEV:
                        out->d_type = DT_BLK;
                        break;

                    case USTAR_DIRECTORY:
                        out->d_type = DT_DIR;
                        break;

                    case USTAR_PIPE:
                        out->d_type = DT_FIFO;
                        break;
                    
                    default:
                        out->d_type = DT_REG;
                        break;
                }

                return out;
            }

            fileidx++;
        }

        // Go to the next one, get the filesize and increment.
        // Remember that GNU devs sometimes get high, so this is encoded in octal.
        uint64_t filesize = strtoull(header.size, NULL, 8);
    
        // Increment ino (512 = sizeof ustar header)
        ino += 512;
        ino += USTAR_SIZE(filesize);

        // Get next header
        if (tarfs_getUstar(node, ino, &header)) {
            break;
        }
    }

    return NULL;
}


/**
 * @brief tarfs readdir method
 */
struct dirent *tarfs_readdir(fs_node_t *node, unsigned long index) {
    // First, if index == 0 or 1, return ./.. respectively
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2; // Start from a 1-index, that way we don't return node. 

    if (!node) return NULL;

    // Get the header and verify it
    uint64_t ino = node->inode;
    ustar_header_t header; 
    tarfs_getUstar(node, ino, &header);
    
    
    // Create the basic search name - files that contain this will be used as indexes
    char search_filename[256];
    memset(search_filename, 0, 256);
    strncat(search_filename, header.nameprefix, 155);
    strncat(search_filename, header.name, 100);
    
    int fileidx = 0; // Index of the current file
    
    // Start iterating
    while (1) {
        // Construct the full filename
        char path_filename[256];
        memset(path_filename, 0, 256);
        strncat(path_filename, header.nameprefix, 155);
        strncat(path_filename, header.name, 150);

        // Count slashes
        int slashes;
        char *s = (char*)path_filename + strlen(search_filename);
        for (slashes=0; s[slashes]; (s[slashes]=='/' && s[slashes+1]) ? slashes++ : *s++);

        // Use strstr to find the occurence of search_filename in path_filename (or if !slashes we checked that earlier)
        if (strstr(path_filename, search_filename) == path_filename && !slashes && strlen((char*)path_filename + strlen(search_filename))) {
            // Match found - is it the one we want?
            if (fileidx == (int)index) {
                // Found the right one!
                struct dirent *out = kmalloc(sizeof(struct dirent));
                memset(out, 0, sizeof(struct dirent));
                out->d_ino = ino;
                
                // Update name to remove last slash
                if (path_filename[strlen(path_filename) - 1] == '/') {
                    path_filename[strlen(path_filename) - 1] = '\0';
                }

                // Copy name
                strncat(out->d_name, path_filename + strlen(search_filename), 256);
    
                // Setup other parameters
                out->d_reclen = strtoull(header.size, NULL, 8);
                
                // Interpret the type now
                switch (header.type[0]) {
                    case USTAR_HARD_LINK:
                        LOG(ERR, "Cannot parse entry '%s' type (USTAR_HARD_LINK) - %s\n", header.name, header.linkname);
                        // Fallthrough
                    case USTAR_SYMLINK:
                        out->d_type = DT_LNK;
                        break;

                    case USTAR_CHARDEV:
                        out->d_type = DT_CHR;
                        break;

                    case USTAR_BLOCKDEV:
                        out->d_type = DT_BLK;
                        break;

                    case USTAR_DIRECTORY:
                        out->d_type = DT_DIR;
                        break;

                    case USTAR_PIPE:
                        out->d_type = DT_FIFO;
                        break;
                    
                    default:
                        out->d_type = DT_REG;
                        break;
                }

                return out;
            }

            fileidx++;
        }

        // Go to the next one, get the filesize and increment.
        // Remember that GNU devs sometimes get high, so this is encoded in octal.
        uint64_t filesize = strtoull(header.size, NULL, 8);
    
        // Increment ino (512 = sizeof ustar header)
        ino += 512;
        ino += USTAR_SIZE(filesize);

        // Get next header
        if (tarfs_getUstar(node, ino, &header)) break;
    }

    return NULL;
}

/**
 * @brief tarfs finddir method
 */
fs_node_t *tarfs_finddir(fs_node_t *node, char *path) {
    if (!node || !path) return NULL;
    uint64_t ino = node->inode;

    ustar_header_t header;
    if (tarfs_getUstar(node, ino, &header)) {
        return NULL;
    }

    if (header.type[0] != USTAR_DIRECTORY) {
        return NULL;
    }
    
    // Create the search filename
    char search_filename[256] = { 0 };

    if (strcmp(header.name, "/")) {
        // We are NOT the root, so concatenate node->name and a slash
        strncat(search_filename, header.nameprefix, 155);
        strncat(search_filename, header.name, 100);
    }  

    strncat(search_filename, path, strlen(path));

    // Start iterating, we'll increment by filesize until we find the appropriate file
    int found  = 0;
    while (1) {
        // tarfs prefixes with a header
        char filename[256];
        snprintf(filename, 256, "%s%s", header.nameprefix, header.name);
        filename[255] = 0;

        // finddir has nodes provide their path without slashes, so remove trailing slashes.
        if (filename[strlen(filename) - 1] == '/' && strlen(filename) > 1) {
            filename[strlen(filename) - 1] = '\0';
        }

        // Is this the file we want?
        if (!strcmp(filename, search_filename)) {
            // Yes, it is - break
            found = 1;
            break;
        }

        // No, get the filesize and increment.
        // Remember that GNU devs sometimes get high, so this is encoded in octal.
        uint64_t filesize = strtoull(header.size, NULL, 8);
    
        // Increment
        ino += 512;
        ino += USTAR_SIZE(filesize);

        // Free header and go to next
        if (tarfs_getUstar(node, ino, &header)) {
            break;
        }
    }

    if (!found) return NULL;

    fs_node_t *retnode = tarfs_ustarToNode(&header, ino, node);
    return retnode;
}




/**
 * @brief Mount a tarfs filesystem
 * @param argp Expects a ramdev device to mount the filesystem based
 */
int tarfs_mount(char *argp, char *mountpoint, fs_node_t **node_out) {
    fs_node_t *tar_file = kopen(argp, O_RDWR);
    if (tar_file == NULL) {
        return -ENODEV; // Failed to open
    }

    // Allocate a new filesystem node for us.
    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));
    
    strcpy(node->name, "tarfs");
    node->flags = VFS_DIRECTORY;
    node->mask = 0770;
    node->dev = tar_file;

    // We can setup the metadata of the root node now
    ustar_header_t header;

    if (tarfs_getUstar(node, 0, &header)) {
        kfree(node);
        return -EINVAL;
    }

    // Now use tarfs_ustarToNode to get the new node
    fs_node_t *rootnode = tarfs_ustarToNode(&header, 0x0, node);
    kfree(node);
    node = rootnode;
    node->readdir = tarfs_readdir_root;

    // All done!
    *node_out = node;
    return 0;
}

/**
 * @brief Initialize the tarfs system
 */
int tarfs_init() {
    vfs_registerFilesystem("tarfs", tarfs_mount);
    return 0;
}

FS_INIT_ROUTINE(tarfs, INIT_FLAG_DEFAULT, tarfs_init);