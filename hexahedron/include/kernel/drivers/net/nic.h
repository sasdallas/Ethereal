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
#include <kernel/fs/vfs.h>
#include <kernel/fs/systemfs.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>

/**** DEFINITIONS ****/

#define NIC_TYPE_ETHERNET           0
#define NIC_TYPE_WIRELESS           1   // lmao, that's totally supported

/* Prefixes */
#define NIC_ETHERNET_PREFIX         "eth%d"
#define NIC_WIRELESS_PRERFIX        "wifi%d"

/* NIC state */
#define NIC_STATE_DOWN              0
#define NIC_STATE_UP                1

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

struct nic;

/**
 * @brief NIC operations
 */
typedef struct nic_ops {
    ssize_t (*send)(struct nic *nic, size_t size, char *buffer);
    int (*ioctl)(struct nic *nic, unsigned long request, void *argp);
} nic_ops_t;

/**
 * @brief NIC structure
 * 
 * Put this structure into your @c dev field on your node.
 */
typedef struct nic {
    char name[128];                     // Name of the NIC
    unsigned char type;                 // Type of the NIC
    nic_stats_t stats;                  // Statistics for the NIC
    size_t mtu;                         // MTU for the NIC
    unsigned char state;                // NIC state (default: UP)
    nic_ops_t *ops;                     // NIC operations

    uint8_t mac[6];                     // MAC address of the NIC
    fs_node_t *parent_node;             // Parent node of the NIC
    void *driver;                       // Driver-defined field

    // TODO: Move this to another structure, perhaps?
    atomic_int ip_id;                   // IP current packet ID
    uint32_t ipv4_address;              // IPv4 address
    uint32_t ipv4_subnet;               // IPv4 subnet
    uint32_t ipv4_gateway;              // IPv4 gateway
} nic_t;

/**** MACROS ****/
#define NIC(node) ((nic_t*)node->dev)

/**** VARIABLES ****/

extern systemfs_node_t *systemfs_net_dir;

/**** INLINES ****/

static inline ssize_t nic_send(nic_t *nic, size_t size, char *buffer) { return nic->ops->send(nic, size, buffer); }

/**** FUNCTIONS ****/

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
nic_t *nic_create(char *name, int type, nic_ops_t *ops, uint8_t *mac, void *driver);

/**
 * @brief Destroy NIC interface
 * @param nic The NIC to destroy
 */
int nic_destroy(nic_t *nic);

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


#endif