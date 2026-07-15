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
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <kernel/init.h>
#include <arpa/inet.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:UDP", __VA_ARGS__)

static ssize_t udp_recvmsg(sock_t *sock, struct msghdr *msg, int flags);
static ssize_t udp_sendmsg(sock_t *sock, struct msghdr *msg, int flags);
static int udp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static int udp_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static int udp_close(sock_t *sock);
static int udp_poll(sock_t *sock, poll_waiter_t *w, poll_events_t events);
static poll_events_t udp_poll_events(sock_t *sock);

static sock_ops_t udp_sock_ops = {
    .sendmsg = udp_sendmsg,
    .recvmsg = udp_recvmsg,
    .accept = NULL,
    .bind = udp_bind,
    .close = udp_close,
    .connect = udp_connect,
    .getpeername = NULL,
    .getsockname = NULL,
    .getsockopt = NULL,
    .setsockopt = NULL,
    .listen = NULL,
    .poll = udp_poll,
    .poll_events = udp_poll_events,
};

/* Hashmap */
hashmap_t *udp_map = NULL;
BITMAP_DEFINE(udp_bitmap, 65535);

/* Global lock */
mutex_t udp_lock = MUTEX_INITIALIZER;

/* Reference counting */
static void udp_free(udp_socket_t *sock);
#define UDP_HOLD(s) refcount_inc(&(s)->refs)
#define UDP_RELEASE(s) if (refcount_dec(&(s)->refs) == 0) udp_free(s)

/* UDP slab */
slab_cache_t *udp_cache = NULL;

/* Misc */
#define UDP_SOCK(s) ((udp_socket_t*)((s)))
#define UDP_DEFAULT_BUFFER_SIZE         32  // Up to 32 packets in queue
#define UDP_CAN_READ(s) ((s)->pkts.info_tail != (s)->pkts.info_head)
#define UDP_ALLOC_START                 49152 // apparently this is the standard range


/**
 * @brief Handle a UDP packet
 */
int udp_handle(nic_t *nic, void *frame, size_t size) {
    ipv4_packet_t *ip_pkt = frame;
    udp_packet_t *pkt = (udp_packet_t*)ip_pkt->payload;

    uint16_t src_port = ntohs(pkt->src_port);
    uint16_t dest_port = ntohs(pkt->dest_port);
    uint16_t len = ntohs(pkt->length);

    len = len - sizeof(udp_packet_t);

    mutex_acquire(&udp_lock);
    udp_socket_t *sck = hashmap_get(udp_map, (void*)(uintptr_t)dest_port);

    if (sck == NULL) {
        LOG(DEBUG, "UDP handler: No socket available for port %04x (src_port %04x)\n", dest_port, src_port);
        mutex_release(&udp_lock);
        return 0;
    }

    UDP_HOLD(sck);
    mutex_release(&udp_lock);


    // Attempt to enqueue a new packet
    mutex_acquire(&sck->lock);
    
    if (ringbuffer_remaining_write(sck->pkts.data) < len) {
        LOG(WARN, "UDP socket ringbuffer is full, dropping %u byte packet\n", len);
        mutex_release(&sck->lock);
        return 0;
    }

    if (sck->pkts.info_tail == sck->pkts.info_head-1) {
        LOG(WARN, "UDP socket packet list is full, dropping %u byte packet\n", len);
        mutex_release(&sck->lock);
        return 0;
    }

    // claim a new packet slot
    udp_packet_info_t *info = &sck->pkts.info[sck->pkts.info_tail];
    sck->pkts.info_tail = (sck->pkts.info_tail + 1) % sck->pkts.info_size;

    info->src_addr = (in_addr_t)ntohl(ip_pkt->src_addr);
    info->src_port = (in_port_t)src_port;
    info->length = len;

    // write content
    ringbuffer_write(sck->pkts.data, (char*)pkt->data, len);
    poll_signal(&sck->event, POLLIN);
    mutex_release(&sck->lock);

    UDP_RELEASE(sck);
    return 0;
}

/**
 * @brief UDP recvmsg method
 */
static ssize_t udp_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);

    if (msg->msg_iovlen == 0) return 0;
    assert(msg->msg_iovlen <= 1);

    mutex_acquire(&udpsock->lock);

    if (udpsock->port == 0) {
        mutex_release(&udpsock->lock);
        return -EINVAL;
    }

    while (!UDP_CAN_READ(udpsock)) {
        if (sock_nonblocking(sock)) {
            mutex_release(&udpsock->lock);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &udpsock->event, POLLIN);
        mutex_release(&udpsock->lock);

        int r = poll_wait(w, -1);

        poll_exit(w);
        poll_destroyWaiter(w);

        if (r != 0) {
            return r;
        }

        mutex_acquire(&udpsock->lock);
    }

    // Claim a packet
    udp_packet_info_t *pkt = &udpsock->pkts.info[udpsock->pkts.info_head];

    size_t pkt_size = pkt->length;
    size_t to_read = msg->msg_iov[0].iov_len;
    if (pkt->length <= to_read) {
        to_read = pkt->length;
    } else {
        LOG(WARN, "Truncating UDP packet from %d bytes to %d bytes\n", pkt->length, to_read);
    }

    ssize_t ret = 0;
    if (msg->msg_flags & MSG_PEEK) {
        ret = ringbuffer_peek(udpsock->pkts.data, msg->msg_iov[0].iov_base, to_read);
    } else {
        ret = ringbuffer_read(udpsock->pkts.data, msg->msg_iov[0].iov_base, to_read);
        
        // if there was a bit more, we need to flush the remaining
        if (pkt_size > to_read) {
            ringbuffer_discard(udpsock->pkts.data, pkt_size-to_read);
        }

        udpsock->pkts.info_head = (udpsock->pkts.info_head+1) % udpsock->pkts.info_size;
    }

    // fill out msg->msg_name if needed
    if (msg->msg_name) {
        assert(msg->msg_namelen >= sizeof(struct sockaddr_in));
        struct sockaddr_in *d = msg->msg_name;
        d->sin_family = AF_INET;
        d->sin_port = htons(pkt->src_port);
        d->sin_addr.s_addr = htonl(pkt->src_addr);
    }

    mutex_release(&udpsock->lock);
    
    // all done, but if they specified MSG_TRUNC we should return the true size
    if (ret < 0 || ((msg->msg_flags & MSG_TRUNC) == 0)) {
        return ret;
    } else {
        return (ssize_t)pkt_size;
    }
}

/**
 * @brief Calculate UDP checksum (standard internet, this should be a utility)
 */
uint16_t udp_checksum(uint32_t src_ip, uint32_t dest_ip, void *udp_pkt, uint16_t udp_len) {
    uint32_t sum = 0;

    typedef struct {
        uint16_t src_port;
        uint16_t dest_port;
        uint16_t length;
        uint16_t checksum;
        uint8_t  data[];
    } __attribute__((packed)) udp_checksum_t;

    udp_checksum_t *hdr = (udp_checksum_t *)udp_pkt;
    hdr->checksum = 0;

    sum += (src_ip & 0xFFFF);
    sum += (src_ip >> 16);
    sum += (dest_ip & 0xFFFF);
    sum += (dest_ip >> 16);
    
    sum += htons(17); 
    sum += htons(udp_len);

    uint16_t *udp_ptr = (uint16_t *)udp_pkt;
    size_t count = udp_len;

    while (count > 1) {
        sum += *udp_ptr++;
        count -= 2;
    }

    if (count > 0) {
        sum += *(const uint8_t *)udp_ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    uint16_t final_checksum = (uint16_t)(~sum);

    if (final_checksum == 0) {
        final_checksum = 0xFFFF;
    }

    return final_checksum;
}

/**
 * @brief UDP sendmsg method
 */
static ssize_t udp_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);

    if (msg->msg_iovlen == 0) return 0;
    assert(msg->msg_iovlen == 1);

    mutex_acquire(&udpsock->lock);

    if (udpsock->conn.sin_port == 0 && msg->msg_name == NULL) {
        mutex_release(&udpsock->lock);
        return -ENOTCONN;
    }

    // udp socket must be bound if not
    if (udpsock->port == 0) {
        // thankfully because this entry is not quite in the map just yet this actually works fine
        mutex_acquire(&udp_lock);
        int prt = bitmap_find_first_from(udp_bitmap, UDP_ALLOC_START, 65535);
        if (prt == -1) {
            LOG(ERR, "Out of UDP ports\n");
            mutex_release(&udp_lock);
            mutex_release(&udpsock->lock);
            return -ENOMEM; // ???
        }

        udpsock->port = (uint16_t)prt;
        hashmap_set(udp_map, (void*)(uintptr_t)udpsock->port, udpsock);
        mutex_release(&udp_lock);
    }

    // Send the packet
    struct sockaddr_in *tgt;
    if (msg->msg_name) {
        assert(msg->msg_namelen >= sizeof(struct sockaddr_in));
        tgt = msg->msg_name;
    } else {
        tgt = &udpsock->conn;
    }

    // !!! a bit racey, but really dont want to be holding a mutex while allocating.
    // !!! only racey as tgt is not a copy of udpsock->conn, and this networking stack has bigger problems than just a thread race
    mutex_release(&udpsock->lock);

    // !!! ideally, the stack should be host-byte order only. network byte ordering is stored in udpsock->conn :P
    // !!! also this is a triple-copy on many NICs, but this is a network stack skill issue
    size_t sz = msg->msg_iov[0].iov_len;
    udp_packet_t *pkt = kmalloc(sizeof(udp_packet_t) + sz);
    pkt->src_port = htons(udpsock->port);
    pkt->dest_port = tgt->sin_port;
    pkt->length = htons(sz+sizeof(udp_packet_t));
    pkt->checksum = 0;

    memcpy(pkt->data, msg->msg_iov[0].iov_base, sz);

    // pkt->checksum = udp_checksum(htonl(udpsock->addr), tgt->sin_addr.s_addr, pkt, sz + sizeof(udp_packet_t));

    nic_t *nic = nic_route(udpsock->addr);
    assert(nic && "???");

    ssize_t ret = ipv4_send(nic, ntohl(tgt->sin_addr.s_addr), IPV4_PROTOCOL_UDP, pkt, sizeof(udp_packet_t)+sz);
    kfree(pkt);

    if (ret < 0) {
        return ret;
    } else {
        return (ssize_t)sz;
    }
}

/**
 * @brief UDP connect method
 */
static int udp_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);

    if (addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    mutex_acquire(&udpsock->lock);
    memcpy(&udpsock->conn, sockaddr, sizeof(struct sockaddr_in));
    mutex_release(&udpsock->lock);

    return 0;
}

/**
 * @brief UDP bind method
 */
static int udp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);
    int r = 0;

    if (addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    // lovely locking pattern
    mutex_acquire(&udp_lock);
    mutex_acquire(&udpsock->lock);

    if (udpsock->port) {
        r = -EINVAL;
        goto _finish;
    }

    struct sockaddr_in *in = (struct sockaddr_in*)sockaddr;
    in_port_t tgt_port = ntohs(in->sin_port);
    in_addr_t tgt_addr = ntohl(in->sin_addr.s_addr);

    if (tgt_port == 0) {
        int prt = bitmap_find_first_from(udp_bitmap, (size_t)tgt_port, 65535);
        assert(prt != -1);
        tgt_port = (in_port_t)prt;
    } else {
        if (bitmap_test(udp_bitmap, tgt_port)) {
            r = -EADDRINUSE;
            goto _finish;
        }
    }

    udpsock->port = tgt_port;
    udpsock->addr = tgt_addr;

    bitmap_set(udp_bitmap, tgt_port);
    hashmap_set(udp_map, (void*)(uintptr_t)tgt_port, udpsock);

_finish:
    mutex_release(&udpsock->lock);
    mutex_release(&udp_lock);
    return r;
}

/**
 * @brief UDP close method
 */
static int udp_close(sock_t *sock) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);
    
    // de-bind the socket if needed
    if (udpsock->port != 0) {
        mutex_acquire(&udp_lock);
        hashmap_remove(udp_map, (void*)(uintptr_t)udpsock->port);
        bitmap_clear(udp_bitmap, udpsock->port);
        mutex_release(&udp_lock);
    }

    // release the initial ref
    UDP_RELEASE(udpsock);
    return 0;
}

/**
 * @brief UDP poll method
 */
static int udp_poll(sock_t *sock, poll_waiter_t *w, poll_events_t events) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);
    return poll_add(w, &udpsock->event, events);
}

/**
 * @brief UDP poll events
 */
static poll_events_t udp_poll_events(sock_t *sock) {
    udp_socket_t *udpsock = UDP_SOCK(sock->driver);
    poll_events_t ret = 0;
    mutex_acquire(&udpsock->lock);

    if (udpsock->pkts.info_head != udpsock->pkts.info_tail) {
        ret |= POLLIN;
    }

    ret |= POLLOUT;

    mutex_release(&udpsock->lock);
    return ret;
}

/**
 * @brief UDP free socket
 */
static void udp_free(udp_socket_t *sock) {
    LOG(DEBUG, "udp_free %p\n", sock);

    ringbuffer_destroy(sock->pkts.data);
    kfree(sock->pkts.info);
    slab_free(udp_cache, sock);
}

/**
 * @brief Create a UDP socket
 */
sock_t *udp_socket() {
    sock_t *sck = socket_allocate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sck->ops = &udp_sock_ops;

    udp_socket_t *udpsock = slab_allocate(udp_cache);
    memset(udpsock, 0, sizeof(udp_socket_t));
    MUTEX_INIT(&udpsock->lock);
    POLL_EVENT_INIT(&udpsock->event);
    refcount_init(&udpsock->refs, 1);

    // initialize socket buffers
    udpsock->pkts.info = kzalloc(sizeof(udp_packet_info_t) * UDP_DEFAULT_BUFFER_SIZE);
    udpsock->pkts.info_head = 0;
    udpsock->pkts.info_tail = 0;
    udpsock->pkts.info_size = UDP_DEFAULT_BUFFER_SIZE;
    udpsock->pkts.data = ringbuffer_create(32 * PAGE_SIZE); // 128KiB

    sck->driver = udpsock;
    return sck;
}

/**
 * @brief Initialize the UDP system
 */
static int udp_init() {
    udp_cache = slab_createCache("udp socket cache", SLAB_CACHE_DEFAULT, sizeof(udp_socket_t), 0, NULL, NULL);
    udp_map = hashmap_create_int("udp port map", 64);
    bitmap_fill(udp_bitmap, 0, 65535);

    return ipv4_register(IPV4_PROTOCOL_UDP, udp_handle);
}

NET_INIT_ROUTINE(udp, INIT_FLAG_DEFAULT, udp_init, ipv4);
