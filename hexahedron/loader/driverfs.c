/**
 * @file hexahedron/loader/driverfs.c
 * @brief Driver filesystem (/kernel/drivers/)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/loader/driver.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <structs/list.h>
#include <kernel/misc/util.h>

/* Driver list */
extern list_t *driver_list;


/**
 * @brief DriverFS read method (/kernel/drivers/XXX/XXX)
 */
static ssize_t driverfs_driverdirRead(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    loaded_driver_t *driver = (loaded_driver_t*)node->dev;

    // Copy to buffer
    char tmp_buffer[512];
    if (node->inode == 0) {
        // "info"
        node->length = snprintf((char*)tmp_buffer, 512,
            "DriverFileName:%s\n"
            "DriverName:%s\n"
            "DriverAuthor:%s\n"
            "Base:%p\n"
            "Size:0x%x\n"
            "Priority:%d\n",
                driver->filename,
                driver->metadata->name,
                driver->metadata->author,
                driver->load_address,
                driver->size,
                driver->priority);
    } else {
        // ???
        node->length = snprintf((char*)tmp_buffer, 512,
            "UnknownFile\n"
            "HowPossible:???\n");
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
 * @brief /kernel/drivers/<ID>/XXX open
 */
void driverfs_driverdirOpen(fs_node_t *node, unsigned int flags) {
    fs_read(node, 0, 0, NULL);
}

/**
 * @brief DriverFS finddir method (/kernel/drivers/XXX)
 */
static fs_node_t *driverfs_driverdirFinddir(fs_node_t *node, char *name) {
    // Resolve driver
    int i = 0;
    loaded_driver_t *driver = NULL;
    foreach(dnode, driver_list) {
        if (i == (int)(uintptr_t)node->dev) {
            driver = (loaded_driver_t*)dnode->value;
            break;
        }
        i++;
    }

    // Did we find one?
    if (!driver) {
        return NULL;
    }

    // Get info
    if (!strcmp(name, "info")) {
        fs_node_t *file = kzalloc(sizeof(fs_node_t));
        strcpy(file->name, "info");
        file->flags = VFS_FILE;
        file->atime = file->mtime = file->ctime = now();
        file->mask = 0777;
        file->open = driverfs_driverdirOpen;
        file->read = driverfs_driverdirRead;
        file->dev = (void*)driver;
        file->inode = 0;
        return file;
    }

    return NULL;
}


/**
 * @brief DriverFS readdir method (/kernel/drivers/XXX)
 */
static struct dirent *driverfs_driverdirReaddir(fs_node_t *node, unsigned long index) {
    // Handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    // Hardcoding ftw
    if (index == 0) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, "info");
        out->d_ino = 1;
        return out;
    }

    return NULL;
}

/**
 * @brief DriverFS finddir method
 */
fs_node_t *driverfs_finddir(fs_node_t *node, char *name) {
    if (!driver_list) return NULL;

    // This is some horrible code.. but, it works ;)
    if (!strncmp(name, "driver", 6)) {
        char *id_ptr = name + 6;
        if (*id_ptr && isdigit(*id_ptr)) {
            int id = strtol(id_ptr, NULL, 10);
            if (id || *id_ptr == '0') {
                fs_node_t *node = kzalloc(sizeof(fs_node_t));
                strncpy(node->name, name, 256);
                node->flags = VFS_DIRECTORY;
                node->atime = node->mtime = node->ctime = now();
                node->mask = 0777;
                node->dev = (void*)(uintptr_t)id;
                node->readdir = driverfs_driverdirReaddir;
                node->finddir = driverfs_driverdirFinddir;
                return node;
            }
        }
    }

    dprintf(INFO, "invalid: %s\n", name);
    return NULL;
}

/**
 * @brief DriverFS readdir method
 */
struct dirent *driverfs_readdir(fs_node_t *node, unsigned long index) {
    // Handle . and ..
    if (index < 2) {
        struct dirent *out = kmalloc(sizeof(struct dirent));
        strcpy(out->d_name, (index == 0) ? "." : "..");
        out->d_ino = 0;
        return out;
    }

    index -= 2;

    // Validations
    if (!driver_list) return NULL;
    if (index >= driver_list->length) return NULL;

    // TODO: This ID system should probably be used more often in the driver system...
    struct dirent *out = kmalloc(sizeof(struct dirent));
    snprintf(out->d_name, 512, "driver%i", index);
    out->d_ino = index;
    return out;
}

/**
 * @brief Mount the drivers /kernel node
 */
void driverfs_init() {
    kernelfs_dir_t *drivers = kernelfs_createDirectory(NULL, "drivers", 0);
    drivers->node->readdir = driverfs_readdir;
    drivers->node->finddir = driverfs_finddir;
}
