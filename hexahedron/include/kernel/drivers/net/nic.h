/**
 * @file hexahedron/include/kernel/drivers/net/nic.h
 * @brief NIC manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_NET_NIC_H
#define DRIVERS_NET_NIC_H

/**** INCLUDES ****/
#include <kernel/drivers/net/ethernet.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/kernelfs.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>

/**** DEFINITIONS ****/

#define NIC_TYPE_ETHERNET           0
#define NIC_TYPE_WIRELESS           1   // lmao, that's totally supported

/* Prefixes */
#define NIC_ETHERNET_PREFIX         "eth%d"
#define NIC_WIRELESS_PRERFIX        "wifi%d"

/**** TYPES ****/

/**
 * @brief Statistics for a NIC
 * 
 * Update these as your driver progresses.
 */
typedef struct nic_stats {
    uint32_t rx_packets;                // Total received packets
    uint32_t rx_dropped;                // Total received dropped packets
    uint32_t rx_bytes;                  // Total received bytes

    uint32_t tx_packets;                // Total transmitted packets
    uint32_t tx_dropped;                // Total transmitted dropped packets
    uint32_t tx_bytes;                  // Total transmitted bytes
} nic_stats_t;

/**
 * @brief NIC structure
 * 
 * Put this structure into your @c dev field on your node.
 */
typedef struct nic {
    char name[128];                     // Name of the NIC
    int type;                           // Type of the NIC
    nic_stats_t stats;                  // Statistics for the NIC

    uint8_t mac[6];                     // MAC address of the NIC
    fs_node_t *parent_node;             // Parent node of the NIC
    void *driver;                       // Driver-defined field

    list_t *raw_sockets;                // Raw sockets for the NIC

    // TODO: Move this to another structure, perhaps
    uint32_t ipv4_address;              // IPv4 address
    uint32_t ipv4_subnet;               // IPv4 subnet
    uint32_t ipv4_gateway;              // IPv4 gateway
} nic_t;

/**** MACROS ****/
#define NIC(node) ((nic_t*)node->dev)

/**** VARIABLES ****/

extern kernelfs_dir_t *kernelfs_net_dir;

/**** FUNCTIONS ****/

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
fs_node_t *nic_create(char *name, uint8_t *mac, int type, void *driver);

/**
 * @brief Register a new NIC to the filesystem
 * @param nic The node of the NIC to register
 * @param interface_name Optional interface name (e.g. "lo") to mount to instead of using the NIC type
 * @returns 0 on success
 */
int nic_register(fs_node_t *nic_device, char *interface_name);

/**
 * @brief Find a NIC device by their node name
 * @param name The name to search for
 * @returns The NIC device on success or NULL on failure
 */
nic_t *nic_find(char *name);

/**
 * @brief Find NIC device by their IPv4 address
 * @param addr The address to search for
 * @returns The NIC device on success or NULL on failure
 */
nic_t *nic_route(in_addr_t addr);

/**
 * @brief Initialize NIC system
 */
void nic_init();

#endif