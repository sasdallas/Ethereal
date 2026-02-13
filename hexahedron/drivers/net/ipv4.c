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
#include <kernel/init.h>
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
            if (arp_search(NIC(nic_node), packet->dest_addr)) {
                LOG_NIC(ERR, nic_node, "Send failed. Could not locate destination.\n");
                return 1;
            }

            entry = arp_get_entry(packet->dest_addr);
            if (!entry) return 1;
        }
    }

    ethernet_send(nic_node->dev, (void*)packet, IPV4_PACKET_TYPE, entry ? entry->hwmac : ETHERNET_BROADCAST_MAC, ntohs(packet->length));

    return 0;
}


/**
 * @brief Send an IPv4 packet
 * @param nic The NIC to send the IPv4 packet to
 * @param dest Destination IPv4 address to send to
 * @param protocol Protocol to send, @c IPV4_PROTOCOL_xxx
 * @param frame Frame to send
 * @param size Size of the frame
 * @returns Size sent or error code
 */
ssize_t ipv4_send(nic_t *nic, in_addr_t dest, uint8_t protocol, void *frame, size_t size) {
    if (!size) return 0;
    
    // First, resolve the IP address

    uint8_t hwmac[6];

    if (dest != IPV4_BROADCAST_IP) {
        arp_table_entry_t *ent = NULL;
        in_addr_t tgt = dest;

        // TODO: idk if this check is correct
        if (nic->ipv4_gateway && (!nic->ipv4_subnet || ((nic->ipv4_subnet & dest) != (nic->ipv4_address & nic->ipv4_subnet)))) {
            ent = arp_get_entry(nic->ipv4_gateway);
            tgt = nic->ipv4_gateway;
            LOG(DEBUG, "path one, ipv4 gateway = %x dst = %x\n", nic->ipv4_gateway, dest);
        } else {
            ent = arp_get_entry(dest);
        }

        if (!ent) {
            if (arp_search(nic, tgt)) {
                LOG(ERR, "Send to IP %x failed (destination could not be located)\n", dest);
                return -ENETUNREACH;
            }

            ent = arp_get_entry(tgt);
            assert(ent);
        }

        memcpy(hwmac, ent->hwmac, 6);
    } else {
        memset(hwmac, 0xff, 6);
    }

    assert(nic->mtu > sizeof(ipv4_packet_t));
    size_t max_per_packet = (nic->mtu - sizeof(ipv4_packet_t)) & ~7;

    // get a new packet ID
    int id = atomic_fetch_add(&nic->ip_id, 1);

    // Now we can chunk and calculate
    size_t offset = 0;
    ssize_t sent = 0;

    while (offset < size) {
        size_t chunk = size - offset;
        if (chunk > max_per_packet) chunk = max_per_packet;
        
        // TODO: efficiency, ofc cant be stack allocated but need to find better way
        ipv4_packet_t *pkt = kmalloc(sizeof(ipv4_packet_t) + chunk);
        memcpy(pkt->payload, (frame + offset), chunk);

        pkt->versionihl = (4 << 4) | 5;
        pkt->dscp = 0;
        pkt->ecn = 0;
        pkt->length = htons(sizeof(ipv4_packet_t) + chunk);
        pkt->id = htons(id);
        pkt->offset = ((offset + chunk < size) ? IPV4_FLAG_MF : 0) | (offset >> 3);
        pkt->src_addr = nic->ipv4_address;
        pkt->dest_addr = dest;
        pkt->protocol = protocol;
        pkt->ttl = 64;
        pkt->checksum = 0;
        pkt->checksum = htons(ipv4_checksum(pkt));

        ssize_t ret = ethernet_send(nic, pkt, IPV4_PACKET_TYPE, hwmac, sizeof(ipv4_packet_t) + chunk);
        if (ret < 0) { 
            kfree(pkt);
            return ret;
        }

        kfree(pkt);
        offset += chunk;
        sent += chunk;
    }

    return sent;

}

/**
 * @brief Handle an IPv4 packet
 * @param frame The frame that was received (does not include the ethernet header)
 * @param nic_node The NIC that got the packet 
 * @param size The size of the packet
 */
int ipv4_handle(void *frame, nic_t *nic, size_t size) {
    ipv4_packet_t *packet = (ipv4_packet_t*)frame;

    // Get addresses
    char dest[17];
    __inet_ntoa(packet->dest_addr, dest);
    char src[17];
    __inet_ntoa(packet->src_addr, src);

    // Handle protocol
    ipv4_handler_t handler = hashmap_get(ipv4_handler_hashmap, (void*)(uintptr_t)packet->protocol);
    if (handler) {
        return handler(nic, (void*)packet, size);
    }

    LOG(WARN, "no handler available for %x\n", packet->protocol);
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

/**
 * @brief Initialize the IPv4 system
 */
static int ipv4_init() {
    ipv4_handler_hashmap = hashmap_create_int("ipv4 handler map", 6);
    ethernet_registerHandler(IPV4_PACKET_TYPE, ipv4_handle);
    socket_register(AF_INET, ipv4_socket);
    return 0;
}

NET_INIT_ROUTINE(ipv4, INIT_FLAG_DEFAULT, ipv4_init, socket);