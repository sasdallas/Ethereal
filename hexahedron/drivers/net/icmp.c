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
#include <kernel/drivers/net/socket.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/clock.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <arpa/inet.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:ICMP", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s]   ICMP: ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)

static ssize_t icmp_sendmsg(sock_t *sock, struct msghdr *msg, int flags);
static ssize_t icmp_recvmsg(sock_t *sock, struct msghdr *msg, int flags);
static sock_ops_t icmp_sock_ops = {
    .sendmsg = icmp_sendmsg,
    .recvmsg = icmp_recvmsg,
    .accept = NULL,
    .bind = NULL,
    .close = NULL,
    .connect = NULL,
    .getpeername = NULL,
    .getsockname = NULL,
    .getsockopt = NULL,
    .setsockopt = NULL,
    .listen = NULL,
    .poll = NULL,
    .poll_events = NULL,
};

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
 * @brief Handle receiving an ICMP packet
 * @param nic The NIC that got the packet 
 * @param frame The frame that was received (IPv4 packet)
 * @param size The size of the packet
 */
int icmp_handle(nic_t *nic, void *frame, size_t size) {
    ipv4_packet_t *ip_packet = (ipv4_packet_t*)frame;
    icmp_packet_t *packet = (icmp_packet_t*)ip_packet->payload;

    if (packet->type == ICMP_ECHO_REQUEST && packet->code == 0) {
        // They are pinging us, respond - try to not waste memory
        printf("Ping request from %s - icmp_seq=%d ttl=%d\n", inet_ntoa((struct in_addr){.s_addr = ip_packet->src_addr}), ntohs(((packet->varies >> 16) & 0xFFFF)), ip_packet->ttl);

        // ipv4_packet_t *resp = kmalloc(ntohs(ip_packet->length));
        // memcpy(resp, ip_packet, ntohs(ip_packet->length));
        // resp->length = ip_packet->length;
        // resp->src_addr = ip_packet->dest_addr;
        // resp->dest_addr = ip_packet->src_addr;
        // resp->ttl = 64;
        // resp->protocol = 1;
        // resp->id = ip_packet->id;
        // resp->offset = htons(0x4000);
        // resp->versionihl = 0x45;
        // resp->dscp = 0;
        // resp->checksum = 0;
        // resp->checksum = htons(ipv4_checksum((ipv4_packet_t*)resp));

        // icmp_packet_t *respicmp = (icmp_packet_t*)resp->payload;
        // respicmp->type = ICMP_ECHO_REPLY;
        // respicmp->code = 0;
        // respicmp->checksum = 0;
        // respicmp->checksum = htons(icmp_checksum((void*)respicmp, ntohs(ip_packet->length) - sizeof(ipv4_packet_t)));
        // ipv4_sendPacket(nic_node, resp);
        // kfree(resp)

        icmp_packet_t *pkt = kmalloc(ntohs(ip_packet->length) - sizeof(ipv4_packet_t));
        memcpy(pkt, packet, ntohs(ip_packet->length) - sizeof(ipv4_packet_t));

        pkt->type = ICMP_ECHO_REPLY;
        pkt->code = 0;
        pkt->checksum = 0;
        pkt->checksum = htons(icmp_checksum(pkt, ntohs(ip_packet->length) - sizeof(ipv4_packet_t)));
        assert(ipv4_send(nic, ntohl(ip_packet->src_addr), IPV4_PROTOCOL_ICMP, pkt, ntohs(ip_packet->length) - sizeof(ipv4_packet_t)) >= 0);
        kfree(pkt);
    } else if (packet->type == ICMP_ECHO_REPLY && packet->code == 0) {
        // We should notify the socket that wanted this
        LOG(DEBUG, "ICMP packet for socket %d\n", ntohs(packet->varies & 0xFFFF));
    
        // Resolve the socket
        sock_t *sock = socket_fromID(ntohs(packet->varies & 0xFFFF));
        if (!sock) {
            LOG(ERR, "Socket not found\n");
            return 0;
        }

        socket_received(sock, frame, size); // Drop entire IPv4 packet
    }

    return 0;
}

/**
 * @brief ICMP sendmsg
 */
static ssize_t icmp_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;

    // Get the message sender
    if (msg->msg_namelen < sizeof(struct sockaddr_in)) return 0;
    struct sockaddr_in *sockaddr = (struct sockaddr_in*)msg->msg_name;

    // Route to a NIC
    nic_t *nic = nic_route(sockaddr->sin_addr.s_addr);
    if (!nic) return -ENETUNREACH; // ??? right code to return?



    ssize_t sent_bytes = 0;
    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        // Check it
        if (msg->msg_iov[i].iov_len < sizeof(icmp_packet_t)) return -EINVAL;

        // Construct a packet to use
        // ipv4_packet_t *pkt = kzalloc(sizeof(ipv4_packet_t) + msg->msg_iov[i].iov_len);
        // pkt->dest_addr = sockaddr->sin_addr.s_addr;
        // pkt->src_addr = nic->ipv4_address;
        // pkt->versionihl = 0x45;
        // pkt->ttl = IPV4_DEFAULT_TTL;
        // pkt->protocol = IPV4_PROTOCOL_ICMP;
        // pkt->offset = htons(0x4000);
        // pkt->length = htons(sizeof(ipv4_packet_t) + msg->msg_iov[i].iov_len);
        // pkt->checksum = 0;
        // pkt->checksum = htons(ipv4_checksum(pkt));
        

        // // Copy in payload data
        // memcpy(pkt->payload, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);

        // // Setup the identifier bits in the ICMP header and the checksum
        // icmp_packet_t *icmp_pkt = (icmp_packet_t*)pkt->payload;
        // icmp_pkt->varies |= htons(sock->id);
        // icmp_pkt->checksum = 0;
        // icmp_pkt->checksum = htons(icmp_checksum((void*)icmp_pkt, msg->msg_iov[i].iov_len));
        
        // ipv4_sendPacket(nic->parent_node, pkt);

        icmp_packet_t *pkt = (icmp_packet_t*)msg->msg_iov[i].iov_base;
        pkt->varies |= htons(sock->id);
        pkt->checksum = 0;
        pkt->checksum = htons(icmp_checksum((void*)pkt, msg->msg_iov[i].iov_len));
        ssize_t r =  ipv4_send(nic, sockaddr->sin_addr.s_addr, IPV4_PROTOCOL_ICMP, pkt, msg->msg_iov[i].iov_len);
        
        if (r < 0) return r;
        sent_bytes += r;
        // kfree(pkt);
    }
    
    return sent_bytes;
}

/**
 * @brief ICMP recvmsg handler
 */
static ssize_t icmp_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    // For each iovec
    ssize_t total_received = 0;

    sock_recv_packet_t *pkt = NULL;

    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        if (pkt) kfree(pkt);

        // Get new packet
        pkt = socket_get(sock);
        if (!pkt) return -EINTR;

        ipv4_packet_t *data = (void*)pkt->data;
        void *icmp_data = (void*)data->payload;

        size_t actual_size = pkt->size - sizeof(ipv4_packet_t);

        // TODO: Handle size mismatch better?
        if (actual_size > msg->msg_iov[i].iov_len) {
            // TODO: Set MSG_TRUNC
            LOG(WARN, "Truncating packet from %d -> %d\n", actual_size, msg->msg_iov[i].iov_len);
            memcpy(msg->msg_iov[i].iov_base, icmp_data, msg->msg_iov[i].iov_len);
            total_received += msg->msg_iov[i].iov_len;
            continue;
        }

        // Copy it and free the packet
        memcpy(msg->msg_iov[i].iov_base, icmp_data, actual_size);
        total_received += actual_size;
    }

    // Need to setup msgname?
    if (msg->msg_namelen == sizeof(struct sockaddr_in)) {
        if (msg->msg_name && pkt) {
            // !!!: Multiple IOVs, diff sources, what?
            struct sockaddr_in *in = (struct sockaddr_in*)msg->msg_name;
            in->sin_port = 0;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = ((ipv4_packet_t*)pkt->data)->src_addr;
        }
    }

    // Free the last data
    if (pkt) kfree(pkt);

    return total_received;
}

/**
 * @brief ICMP socket handler
 */
sock_t *icmp_socket() {
    sock_t *sock = socket_allocate(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    sock->ops = &icmp_sock_ops;
    return sock;
}

/**
 * @brief Initialize and register ICMP
 */
static int icmp_init() {
    return ipv4_register(IPV4_PROTOCOL_ICMP, icmp_handle);
}

NET_INIT_ROUTINE(icmp, INIT_FLAG_DEFAULT, icmp_init, ipv4);