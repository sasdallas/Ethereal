/**
 * @file hexahedron/include/kernel/drivers/net/arp.h
 * @brief Address Resolution Protocol
 * 
 * @ref https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_NET_ARP_H
#define DRIVERS_NET_ARP_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/ethernet.h>
#include <arpa/inet.h>

/**** DEFINITIONS ****/

#define ARP_PACKET_TYPE     0x0806

/* Hardware types */
#define ARP_HTYPE_ETHERNET      1

/* Operation codes */
#define ARP_OPERATION_REQUEST   1
#define ARP_OPERATION_REPLY     2

/* Types for the table */
#define ARP_TYPE_ETHERNET       1

/**** TYPES ****/

/**
 * @brief ARP packet
 */
typedef struct arp_packet {
    uint16_t htype;     // Hardware type
    uint16_t ptype;     // Protocol type
    uint8_t hlen;       // Hardware length
    uint8_t plen;       // Protocol length
    uint16_t oper;      // Operation

    uint8_t sha[6];     // Sender hardware address
    uint32_t spa;       // Sender protocol address
    uint8_t tha[6];     // Target hardware address
    uint32_t tpa;       // Target protocol address
} __attribute__((packed)) arp_packet_t;

/**
 * @brief Table entry
 */
typedef struct arp_table_entry {
    uint32_t    address;    // IP address
    int         hwtype;     // Hardware type
    uint8_t     hwmac[6];   // MAC address
    fs_node_t   *nic;       // NIC
} arp_table_entry_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the ARP system
 */
void arp_init();

/**
 * @brief Get an entry from the cache table
 * @param address The requested address
 * @returns A table entry or NULL
 */
arp_table_entry_t *arp_get_entry(in_addr_t address);

/**
 * @brief Manually add an entry to the cache table
 * @param address The address of the entry
 * @param mac The MAC the entry resolves to
 * @param type The type of the entry, e.g. @c ARP_TYPE_ETHERNET
 * @param nic The NIC the entry corresponds to
 * @returns 0 on success
 */
int arp_add_entry(in_addr_t address, uint8_t *mac, int type, fs_node_t *nic);

/**
 * @brief Remove an entry from the cache table
 * @param address The address to remove
 * @returns 0 on success
 * 
 * @warning The entry is freed upon removal
 */
int arp_remove_entry(in_addr_t address);

/**
 * @brief Request to search for an IP address (non-blocking)
 * @param nic The NIC to search on
 * @param address The IP address of the target
 * 
 * @returns 0 on successful send
 */
int arp_request(fs_node_t *node, in_addr_t address);

/**
 * @brief Request to search for an IP address (blocking)
 * @param nic The NIC to search on
 * @param address The IP address of the target
 * 
 * @returns 0 on success. Timeout, by default, is 20s
 */
int arp_search(fs_node_t *nic, in_addr_t address);


#endif
