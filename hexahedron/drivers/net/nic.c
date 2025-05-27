/**
 * @file hexahedron/drivers/net/nic.c
 * @brief NIC manager
 * 
 * Manages creating, mounting, and unmounting NICs.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/nic.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>

/* NIC list */
list_t *nic_list = NULL;

/* Indexes */
static int net_ethernet_index = 0;
static int net_wireless_index = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:NIC", __VA_ARGS__)

/**
 * @brief Create a new NIC structure
 * @param name Name of the NIC
 * @param mac 6-byte MAC address of the NIC
 * @param type Type of the NIC
 * @param driver Driver-defined field in the NIC. Can be a structure of your choosing
 * @returns A filesystem node, setup methods and go
 * 
 * @note Please remember to setup your NIC's IP address fields
 */
fs_node_t *nic_create(char *name, uint8_t *mac, int type, void *driver) {
    if (!name || !mac) return NULL;
    if (type > NIC_TYPE_WIRELESS) return NULL;

    if (type == NIC_TYPE_WIRELESS) {
        LOG(INFO, "NIC_TYPE_WIRELESS: That's great for you, but we don't support this.\n");
        return NULL;
    }

    // Allocate a NIC
    nic_t *nic = kmalloc(sizeof(nic_t));
    memset(nic, 0, sizeof(nic_t));
    
    // Setup fields
    strncpy(nic->name, name, 128);
    memcpy(nic->mac, mac, 6);
    nic->driver = driver;
    nic->type = type;

    // Allocate a filesystem node
    fs_node_t *node = kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));
    strcpy(node->name, "*BADNIC*");
    node->dev = (void*)nic;
    node->ctime = now();
    node->flags = VFS_BLOCKDEVICE;
    node->mask = 0666;

    nic->parent_node = node;
    
    return node;
}

/**
 * @brief Register a new NIC to the filesystem
 * @param nic The node of the NIC to register
 * @param interface_name Optional interface name (e.g. "lo") to mount to instead of using the NIC type
 * @returns 0 on success
 */
int nic_register(fs_node_t *nic_device, char *interface_name) {
    if (!nic_device || !nic_device->dev) return 1;
    if (!nic_list) nic_list = list_create("nic list");

    // Get the NIC
    nic_t *nic = (nic_t*)nic_device->dev;

    if (interface_name) {
        strncpy(nic_device->name, interface_name, 128);
        goto _name_done;
    }

    switch (nic->type) {
        case NIC_TYPE_ETHERNET:
            snprintf(nic_device->name, 128, NIC_ETHERNET_PREFIX, net_ethernet_index);
            net_ethernet_index++;
            break;
        
        case NIC_TYPE_WIRELESS:
            snprintf(nic_device->name, 128, NIC_WIRELESS_PRERFIX, net_wireless_index);
            net_wireless_index++;
            break;
        
        default:
            LOG(ERR, "Invalid NIC type %d\n", nic->type);
            return 1;
    }

_name_done: ;

    // Setup path
    char fullpath[256];
    snprintf(fullpath, 256, "/device/%s", nic_device->name);

    // Mount it
    if (vfs_mount(nic_device, fullpath) == NULL) {
        LOG(WARN, "Error while mounting NIC \"%s\" to \"%s\"\n", nic->name, fullpath);
        return 1;
    }

    // Append
    list_append(nic_list, (void*)nic);

    LOG(INFO, "Mounted a new NIC \"%s\" to \"%s\"\n", nic->name, fullpath);
    return 0;

}

/**
 * @brief Find a NIC device by their node name
 * @param name The name to search for
 * @returns The NIC device on success or NULL on failure
 */
nic_t *nic_find(char *name) {
    foreach(nic_node, nic_list) {
        nic_t *nic = (nic_t*)nic_node->value;
        if (nic) {
            if (!strcmp(nic->parent_node->name, name)) {
                return nic;
            }
        }
    }

    return NULL;
}