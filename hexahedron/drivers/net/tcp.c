/**
 * @file hexahedron/drivers/net/tcp.c
 * @brief Transmission Control Protocol
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/net/tcp.h>
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

/* CI/CD sucks */
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:TCP", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s]    TCP: ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)

/* Transition TCP port state */
#define TCP_CHANGE_STATE(s) { tcpsock->state = s; LOG(DEBUG, "Socket bound to port %d transition to state \"%s\"\n", tcpsock->port, tcp_stateToString(tcpsock->state)); }

/* Print packet */
#define TCP_PRINT_PKT(pkt, nic, ippkt) LOG_NIC(DEBUG, nic->parent_node, "[%s%s%s%s%s] %d -> %d Seq=%d Ack=%d Len=%d Id=%x\n", \
                                    (ntohs(pkt->flags) & TCP_FLAG_ACK) ? "ACK " : "", \
                                    (ntohs(pkt->flags) & TCP_FLAG_PSH) ? "PSH " : "", \
                                    (ntohs(pkt->flags) & TCP_FLAG_RST) ? "RST " : "", \
                                    (ntohs(pkt->flags) & TCP_FLAG_FIN) ? "FIN " : "", \
                                    (ntohs(pkt->flags) & TCP_FLAG_SYN) ? "SYN" : "", \
                                    ntohs(pkt->src_port), ntohs(pkt->dest_port), ntohl(pkt->seq), ntohl(pkt->ack), ntohs(ippkt->length), ntohs(ippkt->id));

/* Has flag */
#define TCP_HAS_FLAG(pkt, flag) (ntohs(pkt->flags) & TCP_FLAG_##flag)

/* TCP port map */
hashmap_t *tcp_port_map = NULL;

/* UDP port lock */
spinlock_t tcp_port_lock = { 0 };

/* Last port allocated */
static in_port_t tcp_port_last = 2332;

/**
 * @brief TCP state to string
 */
static char *tcp_stateToString(int state) {
    switch (state) {
        case TCP_STATE_DEFAULT:
            return "DEFAULT";
        case TCP_STATE_LISTEN:
            return "LISTEN";
        case TCP_STATE_SYN_SENT:
            return "SYN-SENT";
        case TCP_STATE_SYN_RECV:
            return "SYN-RECV";
        case TCP_STATE_ESTABLISHED:
            return "ESTABLISHED";
        case TCP_STATE_FIN_WAIT1:
            return "FIN-WAIT-1";
        case TCP_STATE_FIN_WAIT2:
            return "FIN-WAIT-2";
        case TCP_STATE_CLOSE_WAIT:
            return "CLOSE-WAIT";
        case TCP_STATE_CLOSING:
            return "CLOSING";
        case TCP_STATE_LAST_ACK:
            return "LAST-ACK";
        case TCP_STATE_CLOSED:
            return "CLOSED";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Perform TCP checksum
 * @param p The TCP checksum header
 * @param h The TCP packet header
 * @param d Data
 * @param size The payload size
 * Source: https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/kernel/net/ipv4.c#L73
 */
uint16_t tcp_checksum(tcp_checksum_header_t *p, tcp_packet_t *h, void *d, size_t size) {
	uint32_t sum = 0;
	uint16_t * s = (uint16_t *)p;

	/* TODO: Checksums for options? */
	for (int i = 0; i < 6; ++i) {
		sum += ntohs(s[i]);
		if (sum > 0xFFFF) {
			sum = (sum >> 16) + (sum & 0xFFFF);
		}
	}

	s = (uint16_t *)h;
	for (int i = 0; i < 10; ++i) {
		sum += ntohs(s[i]);
		if (sum > 0xFFFF) {
			sum = (sum >> 16) + (sum & 0xFFFF);
		}
	}

	uint16_t d_words = size / 2;

	s = (uint16_t *)d;
	for (unsigned int i = 0; i < d_words; ++i) {
		sum += ntohs(s[i]);
		if (sum > 0xFFFF) {
			sum = (sum >> 16) + (sum & 0xFFFF);
		}
	}

	if (d_words * 2 != size) {
		uint8_t * t = (uint8_t *)d;
		uint8_t tmp[2];
		tmp[0] = t[d_words * sizeof(uint16_t)];
		tmp[1] = 0;

		uint16_t * f = (uint16_t *)tmp;

		sum += ntohs(f[0]);
		if (sum > 0xFFFF) {
			sum = (sum >> 16) + (sum & 0xFFFF);
		}
	}

	return ~(sum & 0xFFFF) & 0xFFFF;
}

/**
 * @brief Initialize the TCP system
 */
void tcp_init() {
    tcp_port_map = hashmap_create_int("tcp port map", 20);
    ipv4_register(IPV4_PROTOCOL_TCP, tcp_handle);
}

/**
 * @brief Acknowledge a TCP packet
 * @param nic The NIC that sent the TCP request
 * @param sock The socket to acknowledge the packet on
 * @param ip_pkt The IPv4 packet to acknowledge
 * @param size The size of the payload
 * @returns 0 on a successful acknowledgement
 */
int tcp_acknowledge(nic_t *nic, sock_t *sock, ipv4_packet_t *ip_pkt, size_t size) {
    tcp_packet_t *pkt = (tcp_packet_t*)ip_pkt->payload;
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;

    int retransmit = 0;
    int ret = 0;

    // Check - are we out of order?
    if (tcpsock->ack && !(TCP_HAS_FLAG(pkt, SYN) && TCP_HAS_FLAG(pkt, ACK)) && tcpsock->ack != ntohl(pkt->seq)) {
        // Out of order, their sequence number does not match our acknowledge number.
        // Do a multi-ACK
        retransmit = 1;
        ret = 1; // This wasn't a successful transaction
        LOG(ERR, "TCP retransmission for out-of-order packet\n");
    } else {
        // Not out of order, increase!
        tcpsock->ack = (ntohl(pkt->seq) + size) & 0xFFFFFFFF;
        if (TCP_HAS_FLAG(pkt, SYN) && TCP_HAS_FLAG(pkt, ACK)) tcpsock->seq = 1; // Next sequence number
    }

    // Transmit packet
    ipv4_packet_t *resp_ip = kzalloc(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
    resp_ip->length = htons(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
    resp_ip->src_addr = nic->ipv4_address;
    resp_ip->dest_addr = ip_pkt->src_addr;
    resp_ip->protocol = IPV4_PROTOCOL_TCP;
    resp_ip->ttl = IPV4_DEFAULT_TTL;
    resp_ip->versionihl = 0x45;
    resp_ip->checksum = 0;
    resp_ip->checksum = htons(ipv4_checksum(resp_ip));

    tcp_packet_t *resp = (tcp_packet_t*)resp_ip->payload;
    resp->src_port = htons(tcpsock->port);
    resp->dest_port = pkt->src_port;
    resp->seq = htonl(tcpsock->seq);
    resp->ack = htonl(tcpsock->ack);
    resp->flags = htons(TCP_FLAG_ACK | 0x5000);
    resp->winsz = htons(TCP_DEFAULT_WINSZ);
    
    // Calculate TCP checksum
    tcp_checksum_header_t resp_cksum = {
        .dest = resp_ip->dest_addr,
        .length = htons(sizeof(tcp_packet_t)),
        .protocol = IPV4_PROTOCOL_TCP,
        .reserved = 0,
        .src = resp_ip->src_addr
    };

    resp->checksum = htons(tcp_checksum(&resp_cksum, resp, NULL, 0));

    // Transmit!
    TCP_PRINT_PKT(resp, nic, resp_ip);
    ipv4_sendPacket(nic->parent_node, resp_ip);

    // Retransmit two more times?
    if (retransmit) {
        ipv4_sendPacket(nic->parent_node, resp_ip);
        ipv4_sendPacket(nic->parent_node, resp_ip);
    }

    kfree(resp_ip);
    return ret;
}

/**
 * @brief Send a TCP packet on a socket
 * @param sock The socket to send the TCP packet on
 * @param nic The NIC to send on
 * @param dest The destination address to send to
 * @param tcp_pkt The TCP packet to send (checksum will be calculated)
 * @param data The data to send
 * @param len The length of the data
 * @returns 0 on success
 */
int tcp_sendPacket(sock_t *sock, nic_t *nic, in_addr_t dest, tcp_packet_t *tcp_pkt, void *data, size_t len) {
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;

    // Allocate a new IPv4 packet
    ipv4_packet_t *ip_pkt = kzalloc(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t) + len);
    ip_pkt->src_addr = nic->ipv4_address;
    ip_pkt->dest_addr = dest;
    ip_pkt->protocol = IPV4_PROTOCOL_TCP;
    ip_pkt->id = htons(tcpsock->seq);
    ip_pkt->versionihl = 0x45;
    ip_pkt->length = htons(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t) + len);
    ip_pkt->ttl = IPV4_DEFAULT_TTL;
    ip_pkt->checksum = htons(ipv4_checksum(ip_pkt));

    memcpy(ip_pkt->payload, tcp_pkt, sizeof(tcp_packet_t));
    tcp_packet_t *pkt = (tcp_packet_t*)ip_pkt->payload;
    
    // Copy in the data,
    memcpy(pkt->payload, data, len);

    // Calculate checksum
    tcp_checksum_header_t tcp_cksum = {
        .dest = dest,
        .length = htons(sizeof(tcp_packet_t) + len),
        .protocol = IPV4_PROTOCOL_TCP,
        .reserved = 0,
        .src = nic->ipv4_address
    };
    pkt->checksum = htons(tcp_checksum(&tcp_cksum, pkt, pkt->payload, len));

    // Send it!
    TCP_PRINT_PKT(pkt, nic, ip_pkt);
    int r = ipv4_sendPacket(nic->parent_node, ip_pkt);
    kfree(ip_pkt);
    return r;
}

/**
 * @brief Handle a TCP packet
 * @param nic The NIC the packet came from
 * @param frame The frame inlcluding the IPv4 packet header
 * @param size The size of the packet
 */
int tcp_handle(fs_node_t *nic, void *frame, size_t size) {
    ipv4_packet_t *ip_packet = (ipv4_packet_t*)frame;
    tcp_packet_t *packet = (tcp_packet_t*)ip_packet->payload;

    TCP_PRINT_PKT(packet, NIC(nic), ip_packet);

    // Do we have a port willing to handle this?
    if (hashmap_has(tcp_port_map, (void*)(uintptr_t)ntohs(packet->dest_port))) {
        // Yes, there is a handler
        sock_t *sock = (sock_t*)hashmap_get(tcp_port_map, (void*)(uintptr_t)ntohs(packet->dest_port));

        // TODO: Validate checksum

        // We do still need to acknowledge any packets which were sent. What state is this port in?
        tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
        if (tcpsock->state == TCP_STATE_SYN_SENT) {
            // We are awaiting a SYN-ACK - if this is one then we can acknowledge it
            if (TCP_HAS_FLAG(packet, SYN) && TCP_HAS_FLAG(packet, ACK)) {
                // This is a SYN-ACK, acknowledge request
                if (tcp_acknowledge(NIC(nic), sock, ip_packet, 1) == 0) {
                    // Successful acknowledgement, change state and alert socket
                    TCP_CHANGE_STATE(TCP_STATE_ESTABLISHED);
                    socket_received(sock, (void*)packet, size - sizeof(ipv4_packet_t));
                } else {
                    // Acknowledge failure, I guess just retry?
                    LOG(ERR, "Acknowledgement failure for SYN-ACK\n");
                }
            }

            if (TCP_HAS_FLAG(packet, RST)) {
                // We were awaiting a SYN-ACK but got a RST
                TCP_CHANGE_STATE(TCP_STATE_DEFAULT);
                socket_received(sock, (void*)packet, size - sizeof(ipv4_packet_t));
            }
        } else {
            // Get the packet length and header length
            size_t total_packet_length = ntohs(ip_packet->length) - sizeof(ipv4_packet_t);
            size_t tcp_header_length = ((ntohs(packet->flags) & TCP_HEADER_LENGTH_MASK) >> TCP_HEADER_LENGTH_SHIFT) * sizeof(uint32_t);
            size_t payload_len = total_packet_length - tcp_header_length;

            if (payload_len > 0) {
                // We should ACK this
                // !!!: If this is ALSO an ACK, we need to push the packet twice so our TCP sender can see that it doesn't have to retransmit
                if (TCP_HAS_FLAG(packet, ACK)) {
                    socket_received(sock, (void*)packet, sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
                }

                if (tcp_acknowledge(NIC(nic), sock, ip_packet, payload_len) == 0) {
                    socket_received(sock, (void*)packet, size - sizeof(ipv4_packet_t));
                }
            } else {
                // Final transaction?
                if (TCP_HAS_FLAG(packet, FIN)) {
                    if (tcp_acknowledge(NIC(nic), sock, ip_packet, 0) == 0) {
                        socket_received(sock, (void*)packet, size - sizeof(ipv4_packet_t));
                    }
                } else if (TCP_HAS_FLAG(packet, ACK)) {
                    // We don't ACK an ACK
                    socket_received(sock, (void*)packet, size - sizeof(ipv4_packet_t));
                }
            }
        }
    } else {
        // !!!: Also reply to any SYN-ACK requests to unknown ports with a RST, ACK
        if (TCP_HAS_FLAG(packet, SYN) && TCP_HAS_FLAG(packet, ACK)) {
            LOG_NIC(WARN, nic, "Connection to port %d denied - replying with RST ACK\n", ntohs(packet->dest_port));
            ipv4_packet_t *ip_pkt = kzalloc(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
            ip_pkt->src_addr = NIC(nic)->ipv4_address;
            ip_pkt->dest_addr = ip_packet->src_addr;
            ip_pkt->protocol = IPV4_PROTOCOL_TCP;
            ip_pkt->id = ip_packet->id;
            ip_pkt->versionihl = 0x45;
            ip_pkt->length = htons(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
            ip_pkt->ttl = IPV4_DEFAULT_TTL;
            ip_pkt->checksum = htons(ipv4_checksum(ip_pkt));

            tcp_packet_t *pkt = (tcp_packet_t*)ip_pkt->payload;
            pkt->src_port = packet->dest_port;
            pkt->dest_port = packet->src_port;
            pkt->seq = htonl(1);
            pkt->ack = htonl(1);
            pkt->flags = htons(TCP_FLAG_RST | TCP_FLAG_ACK | 0x5000);
            pkt->winsz = TCP_DEFAULT_WINSZ;

            tcp_checksum_header_t tcp_cksum = {
                .dest = ip_pkt->dest_addr,
                .length = htons(sizeof(tcp_packet_t)),
                .protocol = IPV4_PROTOCOL_TCP,
                .reserved = 0,
                .src = ip_pkt->src_addr
            };

            pkt->checksum = htons(tcp_checksum(&tcp_cksum, pkt, NULL, 0));

            ipv4_sendPacket(nic, ip_pkt);
            kfree(ip_pkt);
        }

        // !!!: Since our port sockets are stupid and are removed before the transactions are actually completed, 
        // !!!: we also should ACK any FIN, ACKs
        if (TCP_HAS_FLAG(packet, FIN) && TCP_HAS_FLAG(packet, ACK)) {
            LOG_NIC(WARN, nic, "Closing connection to port %d (replying with ACK)\n", ntohs(packet->dest_port));
            ipv4_packet_t *ip_pkt = kzalloc(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
            ip_pkt->src_addr = NIC(nic)->ipv4_address;
            ip_pkt->dest_addr = ip_packet->src_addr;
            ip_pkt->protocol = IPV4_PROTOCOL_TCP;
            ip_pkt->id = ip_packet->id;
            ip_pkt->versionihl = 0x45;
            ip_pkt->length = htons(sizeof(ipv4_packet_t) + sizeof(tcp_packet_t));
            ip_pkt->ttl = IPV4_DEFAULT_TTL;
            ip_pkt->checksum = htons(ipv4_checksum(ip_pkt));

            tcp_packet_t *pkt = (tcp_packet_t*)ip_pkt->payload;
            pkt->src_port = packet->dest_port;
            pkt->dest_port = packet->src_port;
            pkt->seq = packet->ack; // ???
            pkt->ack = packet->seq; // ???
            pkt->flags = htons(TCP_FLAG_ACK | 0x5000);
            pkt->winsz = TCP_DEFAULT_WINSZ;

            tcp_checksum_header_t tcp_cksum = {
                .dest = ip_pkt->dest_addr,
                .length = htons(sizeof(tcp_packet_t)),
                .protocol = IPV4_PROTOCOL_TCP,
                .reserved = 0,
                .src = ip_pkt->src_addr
            };

            pkt->checksum = htons(tcp_checksum(&tcp_cksum, pkt, NULL, 0));

            ipv4_sendPacket(nic, ip_pkt);
            kfree(ip_pkt);
        }   
    }
 

    return 0;
}

/**
 * @brief TCP bind method
 * @param sock The socket to bind
 * @param sockaddr The socket address
 * @param addrlen The length of the socket address
 */
int tcp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    // Check to see if this socket is bound already
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    if (tcpsock->port) return -EINVAL; // Already bound
    if (addrlen < sizeof(struct sockaddr_in)) return -EINVAL;

    // Check to see if the port is bound already
    struct sockaddr_in *addr = (struct sockaddr_in*)sockaddr;
    spinlock_acquire(&tcp_port_lock);

    if (hashmap_has(tcp_port_map, (void*)(uintptr_t)ntohs(addr->sin_port))) {
        spinlock_release(&tcp_port_lock);
        return -EADDRINUSE;
    }

    hashmap_set(tcp_port_map, (void*)(uintptr_t)ntohs(addr->sin_port), (void*)sock);
    spinlock_release(&tcp_port_lock);

    tcpsock->port = ntohs(addr->sin_port);
    return 0;
}

/**
 * @brief TCP recvmsg method
 * @param sock The socket to receive the message on
 * @param msg The message to receive
 * @param flags Flags for message receiving
 */
ssize_t tcp_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    if (!sock->connected_addr) return -ENOTCONN;
    if (msg->msg_namelen) return -EISCONN;

    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    if (!tcpsock->port) {
        return -EINVAL; // ??? right error
    }

    ssize_t total_received = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        // Read ACK packet
        sock_recv_packet_t *ack_pkt = socket_get(sock);
        if (!ack_pkt) return -EINTR;
        kfree(ack_pkt);

        sock_recv_packet_t *pkt = socket_get(sock);
        if (!pkt) return -EINTR;

        // Get the data
        tcp_packet_t *tcp_pkt = (tcp_packet_t*)pkt->data;

        size_t actual_size = pkt->size - sizeof(tcp_packet_t);
        if (actual_size > msg->msg_iov[i].iov_len) {
            // TODO: Set MSG_TRUNC and store this data to be reread
            LOG(WARN, "Truncating packet from %d -> %d\n", actual_size, msg->msg_iov[i].iov_len);
            memcpy(msg->msg_iov[i].iov_base, tcp_pkt->payload, msg->msg_iov[i].iov_len);
            total_received += msg->msg_iov[i].iov_len;
            kfree(pkt);
            continue;
        }

        // Copy it
        memcpy(msg->msg_iov[i].iov_base, tcp_pkt->payload, actual_size);
        total_received += actual_size;
        kfree(pkt);
    }

    return total_received;
}

/**
 * @brief TCP sendmsg method
 * @param sock The socket to send the message on
 * @param msg The message to send
 * @param flags Flags for message sending
 */
ssize_t tcp_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    if (!sock->connected_addr) return -ENOTCONN;
    if (msg->msg_namelen) return -EISCONN;

    // Route to a destination NIC
    struct sockaddr_in *in = (struct sockaddr_in*)sock->connected_addr;
    nic_t *nic = nic_route(in->sin_addr.s_addr);
    if (!nic) return -ENETUNREACH;

    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    if (!tcpsock->port) {
        // We have no port?
        // Try to get a port
        spinlock_acquire(&tcp_port_lock);
        while (hashmap_has(tcp_port_map, (void*)(uintptr_t)tcp_port_last)) tcp_port_last++;
        hashmap_set(tcp_port_map, (void*)(uintptr_t)tcp_port_last, (void*)sock);
        tcpsock->port = tcp_port_last;
        spinlock_release(&tcp_port_lock);
    }

    ssize_t total_sent_bytes = 0;
    for (int i = 0; i < msg->msg_iovlen; i++) {
        size_t sent_bytes = 0;

        while (sent_bytes < msg->msg_iov[i].iov_len) {
            struct iovec iov = msg->msg_iov[i];

            // TODO: MTU?
            size_t remain = iov.iov_len - sent_bytes;
            size_t send_size = (remain > 1448) ? 1448 : remain;

            // Make a TCP packet
            tcp_packet_t pkt = {
                .src_port = htons(tcpsock->port),
                .dest_port = in->sin_port,
                .seq = htonl(tcpsock->seq),
                .ack = htonl(tcpsock->ack),
                .flags = htons(TCP_FLAG_PSH | TCP_FLAG_ACK | 0x5000),
                .winsz = htons(TCP_DEFAULT_WINSZ),
                .checksum = 0,
                .urgent = 0,
            };

            // Update sequence number so that when we ACK it will be right
            tcpsock->seq += send_size;

            // Off with the packet
            int handled = 0;
            sleep_untilTime(current_cpu->current_thread, 1, 0);
            fs_wait(sock->node, VFS_EVENT_READ);
            for (int attempts = 0; attempts < 3; attempts++) {
                // Wait for an ACK
                if (!current_cpu->current_thread->sleep) sleep_untilTime(current_cpu->current_thread, 1, 0);
                tcp_sendPacket(sock, nic, in->sin_addr.s_addr, &pkt, iov.iov_base + sent_bytes, remain);
            
                if (!sock->recv_queue->length) {
                    int wakeup = sleep_enter();
                    if (wakeup == WAKEUP_SIGNAL) return -EINTR;
                    if (wakeup == WAKEUP_TIME) {
                        LOG(DEBUG, "Time passed, retrying\n");
                        continue; // !!!: Exit from wait queue
                    }
                } else {
                    sleep_exit(current_cpu->current_thread);
                }

                sock_recv_packet_t *spkt = socket_get(sock);
                if (!spkt) return -EINTR;

                tcp_packet_t *resp_pkt = (tcp_packet_t*)spkt->data;
                if (TCP_HAS_FLAG(resp_pkt, RST)) {
                    // RST!
                    LOG(ERR, "RST packet received - reset handle\n");
                    return -ECONNRESET;
                }  

                // Else, it was probably fine
                sent_bytes += send_size;
                handled = 1;
                break;
            }

            if (!handled) return -ETIMEDOUT; // !!!: Better error code?
        }

        total_sent_bytes += sent_bytes;
    }

    return total_sent_bytes;
}

/**
 * @brief TCP listen method
 * @param sock The socket to listen on
 * @param backlog Amount of allowed pending connections
 */
int tcp_listen(sock_t *sock, int backlog) {
    // TODO: Use backlog
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    TCP_CHANGE_STATE(TCP_STATE_LISTEN);
    return 0;
}

/**
 * @brief TCP accept method
 * @param sock The socket to accept on
 * @param sockaddr The sockaddr structure to place new connection in
 * @param addrlen Pointer to address length to set
 */
int tcp_accept(sock_t *sock, struct sockaddr *sockaddr, socklen_t addrlen) {
    return -EINVAL;
}

/**
 * @brief TCP connect method
 * @param sock The socket to connect
 * @param sockaddr The socket address to connect on
 * @param addrlen The address length of the socket
 */
int tcp_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    if (addrlen < sizeof(struct sockaddr_in)) return -EINVAL;
    struct sockaddr_in *addr = (struct sockaddr_in*)sockaddr;

    // Is the socket already connected?
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    if (tcpsock->state != TCP_STATE_DEFAULT) {
        LOG(ERR, "Cannot connect to new address if socket is already connected\n");
        return -EALREADY;
    }

    // Do we need to get a port?
    if (!tcpsock->port) {
        spinlock_acquire(&tcp_port_lock);
        while (hashmap_has(tcp_port_map, (void*)(uintptr_t)tcp_port_last)) tcp_port_last++;
        hashmap_set(tcp_port_map, (void*)(uintptr_t)tcp_port_last, (void*)sock);
        tcpsock->port = tcp_port_last;
        spinlock_release(&tcp_port_lock);
    }

    // Route to a NIC
    nic_t *nic = nic_route(addr->sin_addr.s_addr);
    if (!nic) return -EHOSTUNREACH; 

    // Construct IPv4 packet
    size_t total_len = sizeof(ipv4_packet_t) + sizeof(tcp_packet_t);

    ipv4_packet_t *ip_packet = kzalloc(total_len);
    ip_packet->length = htons(total_len);
    ip_packet->dest_addr = addr->sin_addr.s_addr;
    ip_packet->src_addr = nic->ipv4_address;
    ip_packet->ttl = IPV4_DEFAULT_TTL;
    ip_packet->protocol = IPV4_PROTOCOL_TCP;
    ip_packet->versionihl = 0x45;

    // Randomize sequence number
    // TODO: identifier should be something else?
    srand(now());
    tcpsock->seq = 0;
    LOG(DEBUG, "TCP socket initial sequence number is %d\n", tcpsock->seq);
    ip_packet->id = htons(tcpsock->seq);

    ip_packet->checksum = 0;
    ip_packet->checksum = htons(ipv4_checksum(ip_packet));

    // Construct TCP packet
    tcp_packet_t *pkt = (tcp_packet_t*)ip_packet->payload;
    pkt->src_port = htons(tcpsock->port);
    pkt->dest_port = addr->sin_port;
    pkt->flags = htons(TCP_FLAG_SYN | 0x5000);
    pkt->winsz = htons(TCP_DEFAULT_WINSZ);

    // Calculate TCP checksum
    tcp_checksum_header_t tcp_cksum_pkt = {
        .src = ip_packet->src_addr,
        .dest = ip_packet->dest_addr,
        .reserved = 0,
        .protocol = IPV4_PROTOCOL_TCP,
        .length = htons(sizeof(tcp_packet_t)),
    };

    pkt->checksum = htons(tcp_checksum(&tcp_cksum_pkt, pkt, NULL, 0));

    // Send TCP packet
    ipv4_sendPacket(nic->parent_node, ip_packet);

    // Change to SYN-SENT state
    TCP_CHANGE_STATE(TCP_STATE_SYN_SENT);

    // Wait for a response
    for (int attempt = 0; attempt < 3; attempt++) {
        // Put the current thread to sleep and let it 
        // (this is basically just a more complicated poll)
        LOG(DEBUG, "Attempt %d of connection\n", attempt);
        sleep_untilTime(current_cpu->current_thread, 1, 0);
        fs_wait(sock->node, VFS_EVENT_READ);
        
        // Sleep!
        int wakeup = sleep_enter();
        if (wakeup == WAKEUP_SIGNAL) {
            kfree(ip_packet);
            return -EINTR;
        }
        
        if (wakeup == WAKEUP_TIME) {
            // Retry
            ipv4_sendPacket(nic->parent_node, ip_packet);
            continue;
        }

        // One thread woke us up! But were we connected?
        if (tcpsock->state == TCP_STATE_DEFAULT) {
            // Connection was closed. We still need to pop from the queue
            sock_recv_packet_t *recv_pkt = socket_get(sock);
            if (!recv_pkt) {
                kfree(ip_packet);
                return -EINTR;
            }

            kfree(recv_pkt); 
            kfree(ip_packet);
            return -ECONNREFUSED;
        }

        // There's still a packet in the queue, grab it and free it
        sock_recv_packet_t *recv_pkt = socket_get(sock);
        if (!recv_pkt) {
            kfree(ip_packet);
            return -EINTR;
        }
        kfree(recv_pkt);
        
        // All connected :D
        LOG(INFO, "Socket at port %d connected successfully\n", tcpsock->port);
        sock->connected_addr = kmalloc(addrlen);
        memcpy(sock->connected_addr, sockaddr, addrlen);
        sock->connected_addr_len = addrlen;
        kfree(ip_packet);
        return 0;
    }

    // Connection failed, we timed out
    kfree(ip_packet); 
    return -ETIMEDOUT;
}

/**
 * @brief TCP close method
 */
int tcp_close(sock_t *sock) {
    tcp_sock_t *tcpsock = (tcp_sock_t*)sock->driver;
    if (!sock->connected_addr || sock->connected_addr_len < sizeof(struct sockaddr_in)) goto _cleanup_socket;

    // We need to do a TCP socket close mechanism
    // Send a FIN packet
    struct sockaddr_in *in = ((struct sockaddr_in*)sock->connected_addr);
    nic_t *nic = nic_route(in->sin_addr.s_addr);
    if (!nic) goto _cleanup_socket;

    tcp_packet_t fin_pkt = {
        .src_port = htons(tcpsock->port),
        .dest_port = in->sin_port,
        .seq = htonl(tcpsock->seq),
        .ack = htonl(tcpsock->ack),
        .flags = htons(TCP_FLAG_FIN | TCP_FLAG_ACK | 0x5000),
        .winsz = htons(TCP_DEFAULT_WINSZ),
        .checksum = 0,
        .urgent = 0
    };

    tcp_sendPacket(sock, nic, in->sin_addr.s_addr, &fin_pkt, NULL, 0);
    TCP_CHANGE_STATE(TCP_STATE_FIN_WAIT1);

_cleanup_socket:
    spinlock_acquire(&tcpsock->pending_lock);
    if (tcpsock->port) hashmap_remove(tcp_port_map, (void*)(uintptr_t)tcpsock->port);
    if (tcpsock->pending_connections) list_destroy(tcpsock->pending_connections, true);
    if (tcpsock->accepting_queue) kfree(tcpsock->accepting_queue);
    kfree(tcpsock);
    return 0;
}

/**
 * @brief Create a TCP socket
 */
sock_t *tcp_socket() {
    sock_t *sock = kzalloc(sizeof(sock_t));
    tcp_sock_t *tcpsock = kzalloc(sizeof(tcp_sock_t));

    sock->sendmsg = tcp_sendmsg;
    sock->recvmsg = tcp_recvmsg;
    sock->bind = tcp_bind;
    sock->connect = tcp_connect;
    sock->close = tcp_close;

    sock->driver = (void*)tcpsock;
    return sock;
}