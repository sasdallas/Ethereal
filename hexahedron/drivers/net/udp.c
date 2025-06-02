/**
 * @file hexahedron/drivers/net/udp.c
 * @brief User Datagram Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/net/udp.h>
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <arpa/inet.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:UDP", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s]    UDP: ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)

/* UDP port map */
hashmap_t *udp_port_map = NULL;

/* UDP port lock */
spinlock_t udp_port_lock = { 0 };

/* Last port allocated */
static in_port_t udp_port_last = 2332;

/**
 * @brief Initialize the UDP system
 */
void udp_init() {
    udp_port_map = hashmap_create_int("udp port map", 20);
    ipv4_register(IPV4_PROTOCOL_UDP, udp_handle);
}

/**
 * @brief Handle a UDP packet
 * @param nic The NIC the packet came from
 * @param frame The frame including the IPv4 packet header
 * @param size The size of the packet
 */
int udp_handle(fs_node_t *nic, void *frame, size_t size) {
    ipv4_packet_t *ip_packet = (ipv4_packet_t*)frame;
    udp_packet_t *packet = (udp_packet_t*)ip_packet->payload;
    
    LOG_NIC(DEBUG, nic, "Receive packet src_port=%d dest_port=%d length=%d\n", ntohs(packet->src_port), ntohs(packet->dest_port), ntohs(packet->length));

    if (hashmap_has(udp_port_map, (void*)(uintptr_t)packet->dest_port)) {
        // We have a handler!
        sock_t *sock = (sock_t*)hashmap_get(udp_port_map, (void*)(uintptr_t)packet->dest_port);
        socket_received(sock, frame, size);
    }

    return 0;
}

/**
 * @brief UDP recvmsg method
 * @param sock The socket to use when receiving
 * @param msg The message to receive
 * @param length The length to receive
 */
ssize_t udp_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;

    // Is it bound?
    udp_sock_t *udpsock = (udp_sock_t*)sock->driver;
    if (!udpsock->port) {
        return -ENOTCONN;
    }

    sock_recv_packet_t *pkt = NULL;

    ssize_t total_received = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        if (pkt) kfree(pkt);

        // Get new packet
        pkt = socket_get(sock);
        if (!pkt) return -EINTR;

        ipv4_packet_t *data = (ipv4_packet_t*)pkt->data;
        udp_packet_t *udp_pkt = (udp_packet_t*)data->payload;

        size_t actual_size = pkt->size - sizeof(ipv4_packet_t) - sizeof(udp_packet_t);
        if (actual_size > msg->msg_iov[i].iov_len) {
            // TODO: Set MSG_TRUNC
            LOG(WARN, "Truncating packet from %d -> %d\n", actual_size, msg->msg_iov[i].iov_len);
            memcpy(msg->msg_iov[i].iov_base, udp_pkt->data, msg->msg_iov[i].iov_len);
            total_received += msg->msg_iov[i].iov_len;
            continue;
        }

        // Copy it
        memcpy(msg->msg_iov[i].iov_base, udp_pkt->data, actual_size);
        total_received += actual_size;
    }


    // Need to setup msg_name?
    if (msg->msg_namelen == sizeof(struct sockaddr_in)) {
        if (msg->msg_name && pkt) {
            // !!!: Multiple IOVs, diff sources, what?
            struct sockaddr_in *in = (struct sockaddr_in*)msg->msg_name;
            in->sin_port = ((udp_packet_t*)((ipv4_packet_t*)pkt->data)->payload)->src_port;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = ((ipv4_packet_t*)pkt->data)->src_addr;
        }
    }

    // Free the last data
    if (pkt) kfree(pkt);

    return total_received;
}

/**
 * @brief UDP sendmsg method
 * @param sock The socket to use when sending the port
 * @param msg The message to send
 * @param length The length to send
 */
ssize_t udp_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    
    // Did they specify a msg_name?
    if (!msg->msg_name) {
        LOG(ERR, "TODO: connected_addr");
        return -EINVAL;
    }

    if (msg->msg_namelen != sizeof(struct sockaddr_in)) return -EINVAL;

    // Is this socket bound yet?
    udp_sock_t *udpsock = (udp_sock_t*)sock->driver;
    if (!udpsock->port) {
        // Try to get a port
        spinlock_acquire(&udp_port_lock);
        while (hashmap_has(udp_port_map, (void*)(uintptr_t)udp_port_last)) udp_port_last++;
        hashmap_set(udp_port_map, (void*)(uintptr_t)udp_port_last, (void*)sock);
        udpsock->port = udp_port_last;
        spinlock_release(&udp_port_lock);
    }

    // Route this to a destination NIC
    struct sockaddr_in *in = (struct sockaddr_in*)msg->msg_name;
    nic_t *nic = nic_route(in->sin_addr.s_addr);
    if (!nic) return -EHOSTUNREACH;

    ssize_t sent_bytes = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        // Construct an IPv4 packet
        ipv4_packet_t *pkt = kzalloc(sizeof(ipv4_packet_t) + sizeof(udp_packet_t) + msg->msg_iov[i].iov_len);
        pkt->protocol = IPV4_PROTOCOL_UDP;
        pkt->length = htons((sizeof(ipv4_packet_t) + sizeof(udp_packet_t) + msg->msg_iov[i].iov_len));
        pkt->dest_addr = in->sin_addr.s_addr;
        pkt->src_addr = nic->ipv4_address;
        pkt->ttl = IPV4_DEFAULT_TTL;
        pkt->offset = htons(0x4000);
        pkt->versionihl = 0x45;
        pkt->checksum = 0;
        pkt->checksum = htons(ipv4_checksum(pkt));

        // UDP packet
        udp_packet_t *udp_pkt = (udp_packet_t*)(pkt->payload);
        udp_pkt->src_port = htons(udpsock->port);
        udp_pkt->dest_port = in->sin_port;
        udp_pkt->length = htons(sizeof(udp_packet_t) + msg->msg_iov[i].iov_len);
        udp_pkt->checksum = 0;

        // Copy the payload
        memcpy(udp_pkt->data, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);

        ipv4_sendPacket(nic->parent_node, pkt);
        kfree(pkt);
        sent_bytes += msg->msg_iov[i].iov_len;
    }

    return sent_bytes;
}

/**
 * @brief UDP bind method
 * @param sock The socket to bind
 * @param sockaddr The address to bind to
 * @param addrlen The length of the address structure
 */
int udp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    // Check to see if this socket is bound already
    udp_sock_t *udpsock = (udp_sock_t*)sock->driver;
    if (udpsock->port) return -EINVAL; // Already bound
    if (addrlen < sizeof(struct sockaddr_in)) return -EINVAL;
    
    // Check to see if the port is bound already
    struct sockaddr_in *addr = (struct sockaddr_in*)sockaddr;
    spinlock_acquire(&udp_port_lock);

    if (hashmap_has(udp_port_map, (void*)(uintptr_t)addr->sin_port)) {
        spinlock_release(&udp_port_lock);
        return -EADDRINUSE;
    }

    hashmap_set(udp_port_map, (void*)(uintptr_t)addr->sin_port, (void*)sock);
    spinlock_release(&udp_port_lock);

    udpsock->port = addr->sin_port;

    // Route the NIC too
    return 0;
}

/**
 * @brief UDP close method
 * @param sock The socket to close
 */
int udp_close(sock_t *sock) {
    udp_sock_t *udpsock = (udp_sock_t*)sock->driver;
    LOG(DEBUG, "Port %d unbound from socket %d\n", udpsock->port, sock->id);

    spinlock_acquire(&udp_port_lock);
    hashmap_remove(udp_port_map, (void*)(uintptr_t)udpsock->port);
    spinlock_release(&udp_port_lock);

    kfree(udpsock);

    return 0;
}

/**
 * @brief Create a UDP socket
 */
sock_t *udp_socket() {
    sock_t *sock = kzalloc(sizeof(sock_t));
    sock->sendmsg = udp_sendmsg;
    sock->recvmsg = udp_recvmsg;
    sock->bind = udp_bind;
    sock->close = udp_close;

    udp_sock_t *udpsock = kzalloc(sizeof(udp_sock_t));
    sock->driver = (void*)udpsock;
    return sock;
}