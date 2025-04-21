/**
 * @file hexahedron/drivers/net/icmp.c
 * @brief Internet Control Message Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/icmp.h>
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <arpa/inet.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:ICMP", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s]   ICMP: ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)

/* Ping packet */
icmp_packet_t *ping_packet = NULL; // ONLY FOR DEBUGGING

/**
 * @brief Initialize and register ICMP
 */
void icmp_init() {
    ipv4_register(IPV4_PROTOCOL_ICMP, icmp_handle);
}

/**
 * @brief Perform an ICMP checksum
 * @param payload ICMP payload
 * @param size ICMP payload size
 */
static uint16_t icmp_checksum(void *payload, size_t len) {
    uint16_t *p = (uint16_t*)payload;
    uint32_t checksum = 0;

    for (size_t i = 0; i < (len / 2); i++) checksum += ntohs(p[i]);
    if (checksum > 0xFFFF) {
        checksum = (checksum >> 16) + (checksum & 0xFFFF);
    }

    return ~(checksum & 0xFFFF) & 0xFFFF;
}

/**
 * @brief Send an ICMP packet
 * @param nic_node The NIC to send the ICMP packet to
 * @param dest_addr Target IP address
 * @param type Type of ICMP packet to send
 * @param code ICMP code to send
 * @param varies What should be put in the 4-byte varies field
 * @param data The data to send
 * @param size Size of packet to send
 * @returns 0 on success
 */
int icmp_send(fs_node_t *nic_node, in_addr_t dest, uint8_t type, uint8_t code, uint32_t varies, void *data, size_t size) {
    if (!data || !size) return 1;

    // Construct ICMP packet
    size_t total_size = sizeof(icmp_packet_t) + size;
    icmp_packet_t *packet = kmalloc(total_size);
    packet->type = type;
    packet->code = code;
    packet->varies = varies;
    memcpy(packet->data, data, size);

    // Checksum
    packet->checksum = 0; // default 0 when calculating
    packet->checksum = htons(icmp_checksum((void*)packet, total_size));

    // Away we go!
    LOG_NIC(DEBUG, nic_node, "Send packet type=%02x code=%02x varies=%08x checksum=%04x\n", packet->type, packet->code, packet->varies, packet->checksum);
    int r = ipv4_send(nic_node, dest, IPV4_PROTOCOL_ICMP, (void*)packet, total_size);
    kfree(packet);
    return r;
}

/**
 * @brief Handle receiving an ICMP packet
 * @param nic_node The NIC that got the packet 
 * @param frame The frame that was received (IPv4 packet)
 * @param size The size of the packet
 */
int icmp_handle(fs_node_t *nic_node, void *frame, size_t size) {
    ipv4_packet_t *ip_packet = (ipv4_packet_t*)frame;
    icmp_packet_t *packet = (icmp_packet_t*)ip_packet->payload;

    LOG_NIC(DEBUG, nic_node, "Receive packet type=%02x code=%02x\n", packet->type, packet->code);

    if (packet->type == ICMP_ECHO_REQUEST && packet->code == 0) {
        // They are pinging us, respond - try to not waste memoryE
        printf("Ping request from %s - icmp_seq=%d ttl=%d\n", inet_ntoa((struct in_addr){.s_addr = ip_packet->src_addr}), ntohs(((packet->varies >> 16) & 0xFFFF)), ip_packet->ttl);

        ipv4_packet_t *resp = kmalloc(ntohs(ip_packet->length));
        memcpy(resp, ip_packet, ntohs(ip_packet->length));
        resp->length = ip_packet->length;
        resp->src_addr = ip_packet->dest_addr;
        resp->dest_addr = ip_packet->src_addr;
        resp->ttl = 64;
        resp->protocol = 1;
        resp->id = ip_packet->id;
        resp->offset = htons(0x4000);
        resp->versionihl = 0x45;
        resp->dscp = 0;
        resp->checksum = 0;
        resp->checksum = htons(ipv4_checksum((ipv4_packet_t*)resp));

        icmp_packet_t *respicmp = (icmp_packet_t*)resp->payload;
        respicmp->type = ICMP_ECHO_REPLY;
        respicmp->code = 0;
        respicmp->checksum = 0;
        respicmp->checksum = htons(icmp_checksum((void*)respicmp, ntohs(ip_packet->length) - sizeof(ipv4_packet_t)));
        ipv4_sendPacket(nic_node, resp);
        kfree(resp);
    } else if (packet->type == ICMP_ECHO_REPLY && packet->code == 0) {
        // Reply to our ping request (DEBUG)
        // We should notify the socket that wanted this
        ping_packet = packet;
    }

    return 0;
}

/**
 * @brief Ping!
 * @param nic_node The NIC to send the ping request on
 * @param addr The address to send the ping request to
 * @returns 0 on success
 */
int icmp_ping(fs_node_t *nic_node, in_addr_t addr) {
    // Start pinging
    char payload[48];
    for (int i = 0; i < 48; i++) {
        payload[i] = i;
    }


    // Setup varies
    uint16_t varies[2];
    varies[0] = 0;

    // Start pinging!
    for (int i = 0; i < 10; i++) {
        varies[1] = htons(i+1);

        uint32_t varies_dword;
        memcpy(&varies_dword, varies, sizeof(uint32_t)); // !!!: grosss..

        // Send
        int send_status = icmp_send(nic_node, addr, ICMP_ECHO_REQUEST, 0, varies_dword, (void*)payload, 48);
        if (send_status) break;

        // Time
        uint64_t start = now();
        while (ping_packet == NULL) asm ("pause");
        uint64_t time = now() - start;

        // Profit
        printf("Response from %s: icmp_seq=%d ttl=64 time=%d ticks\n", inet_ntoa((struct in_addr){.s_addr = addr}), i, time);
        ping_packet = NULL;
    }

    return 0;
}