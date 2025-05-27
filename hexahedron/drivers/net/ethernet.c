/**
 * @file hexahedron/drivers/net/ethernet.c
 * @brief Layer 2 Ethernet handler
 * 
 * This is the layer-2 handler (OSI model) of the Hexahedron networking stack
 * 
 * NICs register themselves with the NIC manager and call @c ethernet_handle to handle
 * received packets or call @c ethernet_send to send packets.
 * 
 * Protocol handlers register themselves as EtherType handlers with @c ethernet_registerHandler
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/mem/alloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <arpa/inet.h>

/* EtherType handler list */
hashmap_t *ethertype_handler_map = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:ETH ", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s] ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)


/**
 * @brief Register a new EtherType handler
 * @param ethertype The EtherType to register
 * @param handler The handler to register
 * @returns 0 on success
 */
int ethernet_registerHandler(uint16_t ethertype, ethertype_handler_t handler) {
    if (!ethertype_handler_map) ethertype_handler_map = hashmap_create_int("ethertype handlers", 20);

    if (hashmap_has(ethertype_handler_map, (void*)(uintptr_t)ethertype)) return 1;
    hashmap_set(ethertype_handler_map, (void*)(uintptr_t)ethertype, (void*)(uintptr_t)handler);
    return 0;
}

/**
 * @brief Unregister an EtherType handler
 * @param ethertype The EtherType to unregister
 */
int ethernet_unregisterHandler(uint16_t ethertype) {
    if (!hashmap_has(ethertype_handler_map, (void*)(uintptr_t)ethertype)) return 1;
    hashmap_remove(ethertype_handler_map, (void*)(uintptr_t)ethertype);
    return 0;
}

/**
 * @brief Handle a packet that was received by an Ethernet device
 * @param packet The ethernet packet that was received
 * @param nic_node The NIC that got the packet
 * @param size The size of the packet
 */
void ethernet_handle(ethernet_packet_t *packet, fs_node_t *nic_node, size_t size) {
    LOG_NIC(DEBUG, nic_node, "ETH: Handle packet type=%04x dst=" MAC_FMT " src=" MAC_FMT "\n", ntohs(packet->ethertype), MAC(packet->destination_mac), MAC(packet->source_mac));

    // Valid packet?
    if (!size) {
        LOG_NIC(ERR, nic_node, "ETH: Invalid size of packet (%d)!", size);
        return;
    }

    // First give it to the sockets
    if (NIC(nic_node)->raw_sockets) {
        foreach(socket_node, NIC(nic_node)->raw_sockets) {
            sock_t *sock = (sock_t*)socket_node->value;
            socket_received(sock, packet, size);
        }
    }

    // Is this packet destined for us? Is it a broad packet?
    if (!memcmp(packet->destination_mac, NIC(nic_node)->mac, 6) || !memcmp(packet->destination_mac, ETHERNET_BROADCAST_MAC, 6)) {
        // Yes, we should handle this packet
        // Try and get an EtherType handler
        ethertype_handler_t handler = (ethertype_handler_t)hashmap_get(ethertype_handler_map, (void*)(uintptr_t)ntohs(packet->ethertype));
        if (handler) {
            if (handler(packet->payload, nic_node, size - sizeof(ethernet_packet_t))) {
                LOG_NIC(ERR, nic_node, "ETH: Failed to handle packet.\n");
            }
        }
    }
}

/**
 * @brief Send a packet to an Ethernet device
 * @param nic_node The NIC to send the packet from
 * @param payload The payload to send
 * @param type The EtherType of the packet to send
 * @param dest_mac The destination MAC address of the packet
 * @param size The size of the packet
 */
void ethernet_send(fs_node_t *nic_node, void *payload, uint16_t type, uint8_t *dest_mac, size_t size) {
    LOG_NIC(DEBUG, nic_node, "ETH: Send packet type=%04x payload=%p dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x size=%d\n", type, payload, MAC(dest_mac), MAC(NIC(nic_node)->mac), size);

    // Allocate a packet
    ethernet_packet_t *pkt = kmalloc(sizeof(ethernet_packet_t) + size);
    memset(pkt, 0, sizeof(ethernet_packet_t) + size);

    memcpy(pkt->payload, payload, size);
    memcpy(pkt->destination_mac, dest_mac, 6);
    memcpy(pkt->source_mac, NIC(nic_node)->mac, 6);
    pkt->ethertype = htons(type);

    // Send the packet!
    fs_write(nic_node, 0, size + sizeof(ethernet_packet_t), (uint8_t*)pkt);

    // Free the packet
    kfree(pkt);
}