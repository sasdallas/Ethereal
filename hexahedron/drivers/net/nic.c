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
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/nicdev.h>
#include <kernel/fs/devfs.h>
#include <kernel/mm/vmm.h>
#include <kernel/task/syscall.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <kernel/init.h>
#include <arpa/inet.h>
#include <errno.h>

/* NIC list */
list_t *nic_list = NULL;
static spinlock_t nic_list_lock = { 0 };

/* Network directory for KernelFS */
kernelfs_dir_t *kernelfs_net_dir = NULL;

/* Cache */
slab_cache_t *nic_cache;

/* devfs */
static int nic_ioctl(devfs_node_t *node, unsigned long request, void *param);
static ssize_t nic_write(devfs_node_t *node, loff_t off, size_t size, const char *buffer);

static devfs_ops_t nic_devfs_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = nic_write,
    .ioctl = nic_ioctl, 
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll = NULL,
    .poll_events = NULL,
};

/* NIC last minor */
atomic_int nic_last_minor = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:NIC", __VA_ARGS__)

/**
 * @brief NIC write
 */
static ssize_t nic_write(devfs_node_t *node, loff_t off, size_t size, const char *buffer) {
    nic_t *nic = node->priv;
    return nic->ops->send(nic, size, (char*)buffer);
}

/**
 * @brief NIC info ioctl
 * @param node The filesystem node
 * @param request The request to use
 * @param param The parameter to use
 * 
 * Only accepts @c IO_NIC ioctls
 */
static int nic_ioctl(devfs_node_t *node, unsigned long request, void *param) {
    nic_info_t *info;
    nic_t *nic = node->priv;
    switch (request) {
        case IO_NIC_GET_INFO:
            SYSCALL_VALIDATE_PTR_SIZE(param, sizeof(nic_info_t));
            info = (nic_info_t*)param;
            strncpy(info->nic_name, nic->name, 128);
            info->nic_ipv4_addr = nic->ipv4_address;
            info->nic_ipv4_gateway = nic->ipv4_gateway;
            info->nic_ipv4_subnet = nic->ipv4_subnet;
            info->nic_mtu = nic->mtu;
            memcpy(info->nic_mac, nic->mac, 6);
            return 0;

        case IO_NIC_SET_INFO:
            SYSCALL_VALIDATE_PTR_SIZE(param, sizeof(nic_info_t));
            info = (nic_info_t*)param;
            nic->ipv4_address = info->nic_ipv4_addr;
            nic->ipv4_subnet = info->nic_ipv4_subnet;
            nic->ipv4_gateway = info->nic_ipv4_gateway;
            nic->mtu = info->nic_mtu;
            return 0;
    }

    if (nic->ops->ioctl) {
        return nic->ops->ioctl(nic, request, param);
    }

    return -EINVAL;
}

/**
 * @brief Create a new NIC structure
 * @param name Name of the NIC
 * @param type Type of the NIC
 * @param ops NIC operations
 * @param mac 6-byte MAC address of the NIC
 * @param driver Driver-defined field in the NIC. Can be a structure of your choosing
 * @returns A filesystem node, setup methods and go
 * 
 * @note Please remember to setup your NIC's IP address fields
 */
nic_t *nic_create(char *name, int type, nic_ops_t *ops, uint8_t *mac, void *driver) {
    nic_t *nic = slab_allocate(nic_cache);
    assert(nic);

    memcpy(nic->name, name, strlen(name));
    nic->type = type;
    nic->ops = ops;
    memcpy(nic->mac, mac, 6);
    nic->driver = driver;
    nic->state = NIC_STATE_UP;
    
    spinlock_acquire(&nic_list_lock);
    list_append(nic_list, nic);
    spinlock_release(&nic_list_lock);

    // Mount with devfs
    assert(devfs_register(devfs_root, name, VFS_BLOCKDEVICE, &nic_devfs_ops, DEVFS_MAJOR_NETWORK, nic_last_minor++, nic));

    return nic;
}

/**
 * @brief Destroy NIC interface
 * @param nic The NIC to destroy
 */
int nic_destroy(nic_t *nic) {
    spinlock_acquire(&nic_list_lock);
    list_delete(nic_list, list_find(nic_list, nic));
    spinlock_release(&nic_list_lock);

    slab_free(nic_cache, nic);
    return 0;
}

/**
 * @brief Read data method for the NIC KernelFS node
 * @param entry KernelFS entry
 * @param data NIC
 */
static int nic_kernelfsRead(kernelfs_entry_t *entry, void *data) {
    nic_t *nic = (nic_t*)data;


    struct in_addr in = { 0 };

    in.s_addr = nic->ipv4_address;
    char *ipv4_addr = strdup(inet_ntoa(in));

    in.s_addr = nic->ipv4_subnet;
    char *ipv4_gateway = strdup(inet_ntoa(in));

    in.s_addr = nic->ipv4_gateway;
    char *ipv4_subnet = strdup(inet_ntoa(in));

    kernelfs_writeData(entry,
                                "Name:%s\n"
                                "Type:%s\n"
                                "MAC:" MAC_FMT "\n"
                                "MTU:%d\n"
                                "Ipv4Address:%s\n"
                                "Ipv4Subnet:%s\n"
                                "Ipv4Gateway:%s\n"
                                "RxCount:%d\n"
                                "RxDropped:%d\n"
                                "RxBytes:%d\n"
                                "TxCount:%d\n"
                                "TxDropped:%d\n"
                                "TxBytes:%d\n",
                                    nic->name,
                                    nic->type == NIC_TYPE_ETHERNET ? "EthernetCard" : "WifiCard",
                                    MAC(nic->mac),
                                    nic->mtu,
                                    ipv4_addr,
                                    ipv4_subnet,
                                    ipv4_gateway,
                                    nic->stats.rx_packets,
                                    nic->stats.rx_dropped,
                                    nic->stats.rx_bytes,
                                    nic->stats.tx_packets,
                                    nic->stats.tx_dropped,
                                    nic->stats.tx_bytes
                                );

    kfree(ipv4_addr);
    kfree(ipv4_subnet);
    kfree(ipv4_gateway);
    return 0;
}


/**
 * @brief Register a new NIC to the filesystem
 * @param nic The node of the NIC to register
 * @param interface_name Optional interface name (e.g. "lo") to mount to instead of using the NIC type
 * @returns 0 on success
 */
int nic_register(fs_node_t *nic_device, char *interface_name) {
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

/**
 * @brief Find NIC device with a valid route to this address
 * @param addr The address to search for
 * @returns The NIC device on success or NULL on failure
 */
nic_t *nic_route(in_addr_t addr) {
    // TODO: This properly
    foreach(nic_node, nic_list) {
        nic_t *nic = (nic_t*)nic_node->value;
        if (nic) {
            if (nic->ipv4_address == addr) return nic; // Trying to find a route to this NIC
        }
    }

    // Return the SECOND node in the list
    if (!nic_list->head->next) return NULL;
    return (nic_t*)nic_list->head->next->value;
}

/**
 * @brief Initialize NIC system
 */
static int nic_init() {
    nic_list = list_create("nic list");
    kernelfs_net_dir = kernelfs_createDirectory(NULL, "net", 1);
    assert((nic_cache = slab_createCache("nic cache", SLAB_CACHE_DEFAULT, sizeof(nic_t), 0, NULL, NULL)));
    return 0;
}

FS_INIT_ROUTINE(nic, INIT_FLAG_DEFAULT, nic_init, kernelfs);