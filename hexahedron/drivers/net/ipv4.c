/**
 * @file hexahedron/drivers/net/ipv4.c
 * @brief Internet Protocol Version 4 handler 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/ipv4.h>
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/arp.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/mm/alloc.h>
#include <structs/hashmap.h>
#include <kernel/debug.h>
#include <string.h>

/* Handler map */
hashmap_t *ipv4_handler_hashmap = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:IPV4", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s]  IPV4: ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)

/* Socket prototypes */
sock_t *ipv4_socket(int type, int protocol);
extern sock_t *icmp_socket();
extern sock_t *udp_socket();
extern sock_t *tcp_socket();

/**
 * @brief Initialize the IPv4 system
 */
void ipv4_init() {
    ipv4_handler_hashmap = hashmap_create_int("ipv4 handler map", 6);
    ethernet_registerHandler(IPV4_PACKET_TYPE, ipv4_handle);
    socket_register(AF_INET, ipv4_socket);
}

/**
 * @brief Register an IPv4 protocol handler
 * @param protocol Protocol to handle
 * @param handler Handler
 * @returns 0 on success
 */
int ipv4_register(uint8_t protocol, ipv4_handler_t handler) {
    if (!ipv4_handler_hashmap) return 1;
    hashmap_set(ipv4_handler_hashmap, (void*)(uintptr_t)protocol, (void*)(uintptr_t)handler);
    return 0;
}

/**
 * @brief Unregister an IPv4 protocol handler
 * @param protocol Protocol to unregister
 * @returns 0 on success
 */
int ipv4_unregister(uint8_t protocol) {
    if (!ipv4_handler_hashmap) return 1;
    hashmap_remove(ipv4_handler_hashmap, (void*)(uintptr_t)protocol);
    return 0;
}


/**
 * @brief inet_ntoa that doesn't use a stack variable
 */
static void __inet_ntoa(const uint32_t src_addr, char * out) {
    uint32_t in_fixed = ntohl(src_addr);
    snprintf(out, 17, "%d.%d.%d.%d",
        (in_fixed >> 24) & 0xFF,
        (in_fixed >> 16) & 0xFF,
        (in_fixed >> 8) & 0xFF,
        (in_fixed >> 0) & 0xFF);
}

/**
 * @brief Calculate the IPv4 checksum
 * @param packet The packet to calculate the checksum of
 * @returns Checksum
 */
uint16_t ipv4_checksum(ipv4_packet_t *packet) {
    uint32_t sum = 0;
	uint16_t * s = (uint16_t *)packet;

	for (int i = 0; i < 10; ++i) {
		sum += ntohs(s[i]);
		if (sum > 0xFFFF) {
			sum = (sum >> 16) + (sum & 0xFFFF);
		}
	}


	return ~(sum & 0xFFFF) & 0xFFFF;
}

/**
 * @brief Send an IPv4 packet
 * @param nic The NIC to send the IPv4 packet to
 * @param packet The packet to send
 * @returns 0 on success
 */
int ipv4_sendPacket(fs_node_t *nic_node, ipv4_packet_t *packet) {
    // Print packet
    char src[17];
    __inet_ntoa(NIC(nic_node)->ipv4_address, src);
    char dst[17];
    __inet_ntoa(packet->dest_addr, dst);
    LOG_NIC(DEBUG, nic_node, "Send packet protocol=%02x ttl=%d cksum=0x%x size=%d src_addr=%s dst_addr=%s\n", packet->protocol, packet->ttl, packet->checksum, packet->length, src, dst);

    // Try to get destination MAC from ARP
    // Is this a local address?
    arp_table_entry_t *entry = NULL;
    if (!NIC(nic_node)->ipv4_subnet || (NIC(nic_node)->ipv4_subnet & packet->dest_addr) != (NIC(nic_node)->ipv4_address & NIC(nic_node)->ipv4_subnet)) {
        entry = arp_get_entry(NIC(nic_node)->ipv4_gateway);
    } else {
        entry = arp_get_entry(packet->dest_addr);
        if (!entry) {
            if (arp_search(nic_node, packet->dest_addr)) {
                LOG_NIC(ERR, nic_node, "Send failed. Could not locate destination.\n");
                return 1;
            }

            entry = arp_get_entry(packet->dest_addr);
            if (!entry) return 1;
        }
    }

    ethernet_send(nic_node, (void*)packet, IPV4_PACKET_TYPE, entry ? entry->hwmac : ETHERNET_BROADCAST_MAC, ntohs(packet->length));

    return 0;
}


/**
 * @brief Send an IPv4 packet
 * @param nic The NIC to send the IPv4 packet to
 * @param dest Destination IPv4 address to send to
 * @param protocol Protocol to send, @c IPV4_PROTOCOL_xxx
 * @param frame Frame to send
 * @param size Size of the frame
 * @returns 0 on successful send
 */
int ipv4_send(fs_node_t *nic_node, in_addr_t dest, uint8_t protocol, void *frame, size_t size) {
    if (!nic_node || !NIC(nic_node) || !frame || !size) return 0;
    nic_t *nic = NIC(nic_node);
    
    // Allocate a packet
    size_t total_size = sizeof(ipv4_packet_t) + size;
    ipv4_packet_t *packet = kmalloc(total_size);
    memset(packet, 0, sizeof(ipv4_packet_t));

    // Copy data in
    memcpy(packet->payload, frame, size);

    // Setup packet fields
    packet->length = htons(total_size);
    packet->dest_addr = dest;
    packet->src_addr = nic->ipv4_address;
    packet->ttl = IPV4_DEFAULT_TTL;
    packet->offset = (protocol == IPV4_PROTOCOL_ICMP) ? htons(0x4000) : htons(0x0); // TODO: Fix this
    packet->id = htons(0x0); // TODO
    packet->versionihl = 0x45;
    packet->dscp = htons(0x0); // TODO
    packet->ecn = htons(0x0); // TODO
    packet->protocol = protocol;


    packet->checksum = 0;
    packet->checksum = htons(ipv4_checksum(packet));

    // Send!
    int r = ipv4_sendPacket(nic_node, packet);    
    kfree(packet);

    return r;
}

/**
 * @brief Handle an IPv4 packet
 * @param frame The frame that was received (does not include the ethernet header)
 * @param nic_node The NIC that got the packet 
 * @param size The size of the packet
 */
int ipv4_handle(void *frame, fs_node_t *nic_node, size_t size) {
    ipv4_packet_t *packet = (ipv4_packet_t*)frame;

    // Get addresses
    char dest[17];
    __inet_ntoa(packet->dest_addr, dest);
    char src[17];
    __inet_ntoa(packet->src_addr, src);


    LOG_NIC(DEBUG, nic_node, "Handle packet protocol=%02x ttl=%d length=%d dest=%s src=%s\n", packet->protocol, packet->ttl, ntohs(packet->length), dest, src);

    // Handle protocol
    ipv4_handler_t handler = hashmap_get(ipv4_handler_hashmap, (void*)(uintptr_t)packet->protocol);
    if (handler) {
        return handler(nic_node, (void*)packet, size);
    }

    return 0;
}

/**
 * @brief IPv4 create socket method
 */
sock_t *ipv4_socket(int type, int protocol) {
    // !!!: Temporary? Do we want to modularize TCP, UDP, and/or ICMP?
    switch (type) {
        case SOCK_DGRAM:
            if (protocol == IPPROTO_ICMP) return icmp_socket();
            if (!protocol || protocol == IPPROTO_UDP) return udp_socket();
            return NULL;
        case SOCK_STREAM:
            if (protocol == IPPROTO_TCP || !protocol) return tcp_socket();
            return NULL;
        default:
            return NULL;
    };
}