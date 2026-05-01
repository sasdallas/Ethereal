/**
 * @file hexahedron/drivers/net/tcp.c
 * @brief Transmission Control Protocol
 * 
 * Hexahedron's TCP stack is implemented using worker threads.
 * send/recv calls will send a request to the worker threads and then poll their events.
 * 
 * @ref https://datatracker.ietf.org/doc/html/rfc793
 * 
 * @todo Implement window scaling for us, we support it for remote hosts but for now we are stuck with 65535
 * @todo Segment retransmission
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
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <structs/ringbuffer.h>
#include <structs/queue.h>
#include <structs/queue_rb.h>
#include <kernel/misc/hash.h>
#include <kernel/init.h>
#include <kernel/fs/poll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

/* TCP socket */
typedef struct tcp_tcb {
    in_port_t port;         // Bound port for the TCB. Cannot be 0
    tcp_state_t state;      // standard TCP socket state
    poll_event_t event;     // TCB poll event for connections/listens/etc.
                            // Usage of this depends on the current TCB state, but in general you should prefer the send/recv events.
    refcount_t ref;         // TCB_HOLD, TCB_RELEASE
    mutex_t lck;            // Lock for managing things like state, port, connection, etc.
    bool awaiting_flush;    // Awaiting flush

    char *transmit_buffer;  // Transmission buffer

    struct {
        in_addr_t target_addr;
        in_port_t target_port;
    } connect_info;

    poll_event_t wndev;     // Window event, standard POLLIN/POLLOUT/POLLERR

    struct {
        mutex_t sndlck;
        ringbuffer_t *buffer;

        // https://datatracker.ietf.org/doc/html/rfc793 3.2
        uint32_t snduna;            // SND.UNA (send unacknowledged)
        uint32_t sndwnd;            // SND.WIN (current usable window size)
        uint32_t sndnxt;            // SND.NXT (next sequence number to send)
        uint32_t sndup;             // SND.UP (send urgent pointer)
        uint32_t sndwl1;            // SND.WL1 (segment sequence number used for last window update)
        uint32_t sndwl2;            // SND.WL2 (segment ack number used for last window number)
        uint8_t sndscale;           // Window scaling option
        uint16_t sndmss;            // Send MSS
    } send_window;

    // Receive window
    struct {
        mutex_t rcvlck;
        ringbuffer_t *buffer;

        // https://datatracker.ietf.org/doc/html/rfc793 3.2
        uint32_t rcvnxt;            // RCV.NXT (next expected sequence number)
        uint32_t rcvwnd;            // RCV.WND (advertised window scale)
        uint32_t rcvup;             // RCV.UP (receive urgent pointer)
        uint8_t rcvscale;           // Window scaling option
    } recv_window;

    queue_t *pending_connections;
    
    // TODO: replace this with a hashmap
    struct tcp_tcb *sub_link; // either next on normal TCBs or the first subchild on listener TCBs
    struct tcp_tcb *parent; // parented listener connection
} tcp_tcb_t;

#define TCP_TCB(d) ((tcp_tcb_t*)d)

static void tcp_freeTCB(tcp_tcb_t *tcb);
#define TCB_HOLD(tcb) refcount_inc(&(tcb)->ref)
#define TCB_RELEASE(tcb) if (refcount_dec(&(tcb)->ref) == 1) { tcp_freeTCB(tcb); }

#define TCB_LOCK(tcb) mutex_acquire(&(tcb)->lck)
#define TCB_UNLOCK(tcb) mutex_release(&(tcb)->lck)
#define TCB_LOCK_SND(tcb) mutex_acquire(&(tcb)->send_window.sndlck)
#define TCB_UNLOCK_SND(tcb) mutex_release(&(tcb)->send_window.sndlck)
#define TCB_LOCK_RCV(tcb) mutex_acquire(&(tcb)->recv_window.rcvlck)
#define TCB_UNLOCK_RCV(tcb) mutex_release(&(tcb)->recv_window.rcvlck)


/* TCP socket hashmap */
hashmap_t *tcp_port_map = NULL;
mutex_t tcp_port_map_mutex = MUTEX_INITIALIZER;

/* Port bitmap */
uint64_t tcp_port_bitmap[65535 / (sizeof(uint64_t) * 8)] = { 0 };
spinlock_t tcp_port_lock = SPINLOCK_INITIALIZER;
#define TCP_DEFAULT_PORT    2332

#define TCP_PORT_GET(p) (tcp_port_bitmap[(p) / 64] & (1 << (p % 64)))
#define TCP_PORT_SET(p, val) (val ? (tcp_port_bitmap[(p) / 64] |= (1 << (p % 64))) : (tcp_port_bitmap[(p) / 64] &= ~(1 << (p % 64))))

/* TCB slab */
slab_cache_t *tcb_slab = NULL;

/* Default window size */
#define TCP_DFL_WINDOW_SIZE         65535
#define TCP_DFL_WINDOW_SHIFT        0

/* TCP options */
#define TCP_THREADS                 10

#define TCP_WORKER_BUFFER_SIZE      (TCP_DFL_WINDOW_SIZE << TCP_DFL_WINDOW_SHIFT) + sizeof(unsigned char)
#define TCP_WORKER_PROCESS_PACKET   0 // Process a packet
#define TCP_WORKER_CLOSE            1 // Close the connection
#define TCP_WORKER_FLUSH_SEND       2 // Flush the send window

typedef struct tcp_worker_request {
    unsigned char request_type;
    tcp_tcb_t *tcb;
    char data[0];
} tcp_worker_request_t;

typedef struct tcp_worker {
    // requests are prefixed with their unsigned char type
    
    thread_t *thr;
    queue_rb_t request_queue;
    volatile size_t requests;
    mutex_t lock;
} tcp_worker_t;

tcp_worker_t tcp_workers[TCP_THREADS] = { 0 };
process_t *tcp_process;

/* TCP socket ops */
static int tcp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static int tcp_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen);
static ssize_t tcp_sendmsg(sock_t *sock, struct msghdr *msg, int flags);
static ssize_t tcp_recvmsg(sock_t *sock, struct msghdr *msg, int flags);
static int tcp_poll(sock_t *sock, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t tcp_poll_events(sock_t *sock);
static int tcp_close(sock_t *sock);
static int tcp_listen(sock_t *sock, int backlog);
static int tcp_accept(struct sock *sock, struct sockaddr *addr, socklen_t *addrlen);


sock_ops_t tcp_socket_ops = {
    .accept = tcp_accept,
    .bind = tcp_bind,
    .close = tcp_close,
    .connect = tcp_connect,
    .getpeername = NULL,
    .getsockname = NULL,
    .getsockopt = NULL,
    .setsockopt = NULL,
    .listen = tcp_listen,
    .poll = tcp_poll,
    .poll_events = tcp_poll_events,
    .sendmsg = tcp_sendmsg,
    .recvmsg = tcp_recvmsg,
};

/* Protos */
tcp_tcb_t *tcp_findTCB(in_port_t port, in_addr_t remote_host, in_port_t remote_port);
void tcp_initTCB(tcp_tcb_t *tcb);

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NET:TCP", __VA_ARGS__)


/**
 * @brief TCP checksum
 * Returns as network byte ordered
 */
uint16_t tcp_checksum(tcp_checksum_psuedo_t *psuedo, tcp_packet_t *pkt) {
    uint32_t sum = 0;
    uint16_t *ptr;
    int len;

    ptr = (uint16_t *)psuedo;
    len = sizeof(tcp_checksum_psuedo_t);
    while (len > 0) {
        sum += *ptr++;
        len -= 2;
    }

    ptr = (uint16_t *)pkt;
    len = ntohs(psuedo->length);

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len > 0) {
        sum += (uint16_t)(*(uint8_t *)ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

/**
 * @brief TCP init packet
 * @param packet The packet to initialize
 * @param tcb The TCB to use
 * @param length The length of the packet's data
 * @param flags Packet flags
 */
static void tcp_initPacket(tcp_packet_t *pkt, tcp_tcb_t *tcb, size_t length, uint8_t flags)  {
    pkt->src_port = htons(tcb->port);
    pkt->dst_port = htons(tcb->connect_info.target_port);
    pkt->seq = htonl(tcb->send_window.sndnxt);
    if (flags & TCP_ACK) pkt->ack = htonl(tcb->recv_window.rcvnxt);
    else pkt->ack = 0;
    pkt->reserved = 0;
    pkt->data_offset = (sizeof(tcp_packet_t)/4);
    pkt->flags = flags;
    pkt->window_size = htons(tcb->recv_window.rcvwnd);
    pkt->checksum = 0;
    pkt->urgent_pointer = 0; 
}

/**
 * @brief TCP send packet
 */
static int tcp_sendPacket(tcp_tcb_t *tcb, nic_t *nic, in_addr_t target, tcp_packet_t *pkt, size_t size) {
    // calculate the TCP checksum
    pkt->checksum = 0;
    
    tcp_checksum_psuedo_t psuedo = {
        .src = nic->ipv4_address,
        .dst = htonl(target),
        .length = htons(size),
        .protocol = IPV4_PROTOCOL_TCP,
        .reserved = 0
    };

    pkt->checksum = tcp_checksum(&psuedo, pkt);
    int err = ipv4_send(nic, htonl(target), IPV4_PROTOCOL_TCP, pkt, size);
    return (err < 0) ? err : 0;
}

/**
 * @brief TCP send acknowledgement
 */
static int tcp_acknowledge(tcp_tcb_t *tcb) {
    // Acknowledge the latest packet
    tcp_packet_t pkt;
    tcp_initPacket(&pkt, tcb, 0, TCP_ACK);

    // To the default connection
    nic_t *nic = nic_route(tcb->connect_info.target_addr);
    assert(nic);
    return tcp_sendPacket(tcb, nic, tcb->connect_info.target_addr, &pkt, sizeof(tcp_packet_t));
}

/**
 * @brief Reset the TCP connection via a RST(-ACK)
 */
static int tcp_reset(tcp_tcb_t *tcb, in_addr_t ip, tcp_packet_t *offender) {
    tcp_packet_t pkt = { 0 };

    nic_t *nic = nic_route(ip);
    if (!nic) return -EHOSTUNREACH;

    // Manually init packet
    if (!tcb) {
        pkt.src_port = offender->dst_port;
        pkt.dst_port = offender->src_port;
        pkt.seq = 0; // ignored during a rst-ack
        pkt.ack = htonl(ntohl(offender->seq) + 1);
        pkt.window_size = htons(TCP_DFL_WINDOW_SIZE);
        pkt.flags = TCP_RST | TCP_ACK;
        pkt.data_offset = (sizeof(tcp_packet_t)/4);
    } else {
        tcp_initPacket(&pkt, tcb, 0, TCP_RST);
    }

    return tcp_sendPacket(tcb, nic, ip, &pkt, sizeof(tcp_packet_t));
}



/**
 * @brief Flush a single packet out into the big unknown
 * TCB should be send-locked
 * 
 * Transmits one packet of tcb->send_window.mss
 */
int tcp_transmit(tcp_tcb_t *tcb) {
    uint32_t send = min(tcb->send_window.sndmss, tcb->send_window.sndwnd);

    ssize_t size = ringbuffer_read(tcb->send_window.buffer, &tcb->transmit_buffer[sizeof(tcp_packet_t)], send);
    if (!size) return 0;

    tcp_packet_t *pkt = (tcp_packet_t*)tcb->transmit_buffer;
    LOG(DEBUG, "transmitting segment length=%d\n", size);
    tcp_initPacket(pkt, tcb, size, TCP_PSH | TCP_ACK);

    // TODO RETRANSMISSION!!!!!!!!!!!!!!!!!!!!!!
    nic_t *tgt = nic_route(tcb->connect_info.target_addr);
    assert(tgt);
    int r =  tcp_sendPacket(tcb, tgt, tcb->connect_info.target_addr, pkt, size+sizeof(tcp_packet_t));
    if (!r) {
        tcb->send_window.sndnxt += size;
        tcb->send_window.sndwnd -= size; // rthis is speculative and the actual window size will be updated next ACK 
    }
    return r;
}

/**
 * @brief Parse packet options
 */
void tcp_parsePacketOptions(tcp_packet_t *pkt, uint8_t *scale, uint16_t *mss) {
    if (pkt->data_offset == sizeof(tcp_packet_t)/4) {
        // No options to parse, use defaults
        *scale = 0;
        *mss = 536; // IPv4 MSS
        return;
    }

    int noptions = pkt->data_offset * 4 - sizeof(tcp_packet_t);

    int i = 0;
    while (i < noptions) {
        switch (pkt->options[i]) {
            case TCP_OPT_END:
                return;

            case TCP_OPT_NOP:
                i++;
                break;

            case TCP_OPT_MSS:
                i += 2;
                *mss = ntohs(*(uint16_t*)&pkt->options[i]);
                i += 2;
                break;

            case TCP_OPT_WS:
                i += 2;
                *scale = pkt->options[i];
                i++;
                break;

            default:
                LOG(WARN, "TCP option %d is not implemented yet\n", pkt->options[i]);
                i += 1;
                i += pkt->options[i];
                break;
        }
    }
}

/**
 * @brief Find the least loaded worker for a request, returning locked
 */
static tcp_worker_t *tcp_findWorker(unsigned char request_type) {
    // TODO: Check how good this actually is
    tcp_worker_t *best = NULL;
    for (int i = 0; i < TCP_THREADS; i++) {
        tcp_worker_t *worker = &tcp_workers[i];
        
        // Find the least loaded worker
        if (!best || (worker->requests < best->requests)) {
            best = worker;
        }
    }
    
    // TODO: could be improved further
    if (best) mutex_acquire(&best->lock);
    return best;
}

/**
 * @brief Send worker a request
 */
static void tcp_requestWorker(tcp_worker_t *worker, tcp_tcb_t *tcb, unsigned char request_type, void *request, size_t request_size) {
    if (tcb) TCB_HOLD(tcb);
    dprintf(DEBUG, "TCB Worker Request %x\n", request_type);
    tcp_worker_request_t *req = kmalloc(sizeof(tcp_worker_request_t) + request_size);
    req->request_type = request_type;
    req->tcb = tcb;
    if (request_size) memcpy(req->data, request, request_size);

    queue_rb_push(&worker->request_queue, req);
    worker->requests++;
    sleep_wakeup(worker->thr);
}

/**
 * @brief Finish with worker
 */
void tcp_finishWorker(tcp_worker_t *worker) {
    mutex_release(&worker->lock);
}

/**
 * @brief Worker process packet
 */
void tcp_workerProcessPacket(tcp_worker_t *self, tcp_worker_request_t *request) {
    ipv4_packet_t *ip_pkt = (ipv4_packet_t*)request->data;
    tcp_packet_t *tcp_pkt = (tcp_packet_t*)ip_pkt->payload;

    if (!request->tcb) {
        // A TCB was not specified, therefore nothing is bound to the port.
        // Send a RST back
        tcp_reset(NULL, ntohl(ip_pkt->src_addr), tcp_pkt);
        return;
    }


    tcp_tcb_t *tcb = request->tcb;
    TCB_LOCK(tcb);

    switch (tcb->state) {
        case TCP_STATE_LISTEN: {
            // Listener expects a SYN
            if (tcp_pkt->flags != (TCP_SYN)) {
                LOG(WARN, "Expected SYN but got %x\n", tcp_pkt->flags);
                tcp_reset(NULL, ntohl(ip_pkt->src_addr), tcp_pkt);
                break;
            }

            // Ok we got a SYN, let's prepare a new TCB for the child.
            tcp_tcb_t *new = slab_allocate(tcb_slab);
            assert(new);
            memset(new, 0, sizeof(tcp_tcb_t));

            // Initialize the new TCB
            tcp_initTCB(new);
            
            // Hold an extra ref on the existing TCB which will be released later
            TCB_HOLD(tcb);

            TCB_LOCK(new);
            new->connect_info.target_addr = ntohl(ip_pkt->src_addr);
            new->connect_info.target_port = ntohs(tcp_pkt->src_port);
            new->state = TCP_STATE_SYN_RECEIVED;
            new->port = tcb->port;
            new->parent = tcb;

            // Do not notify the parent-TCB just yet but put it in the list so we can get the final ACK
            new->sub_link = tcb->sub_link;
            tcb->sub_link = new;

            LOG(DEBUG, "Got a new connection on from port %d\n", ntohs(tcp_pkt->src_port));

            // SYN-ACK now
            uint16_t mss;
            uint8_t wscale;
            tcp_parsePacketOptions(tcp_pkt, &wscale, &mss);

            LOG(DEBUG, "Parsed MSS %04x Wscale %x\n", mss, wscale);

            nic_t *nic = nic_route(ntohl(ip_pkt->src_addr));
            assert(nic);

    
            new->send_window.sndmss = nic->mtu - sizeof(ipv4_packet_t) - sizeof(tcp_packet_t);
            new->send_window.sndwnd = ntohs(tcp_pkt->window_size);
            new->send_window.sndscale = wscale;
            new->send_window.sndwl1 = ntohl(tcp_pkt->seq);
            new->send_window.sndwl2 = ntohl(tcp_pkt->ack);
            new->send_window.sndmss = min(mss, new->send_window.sndmss); // SNDMSS was already setup by tcp_connect as the NIC's MTU
            new->recv_window.rcvnxt = new->send_window.sndwl1 + 1;
            new->recv_window.rcvwnd = TCP_DFL_WINDOW_SIZE;

            new->transmit_buffer = kmalloc(new->send_window.sndmss);

            // TODO: SEND THE MSS
            tcp_packet_t pkt;
            tcp_initPacket(&pkt, new, 0, TCP_SYN | TCP_ACK);
            tcp_sendPacket(new, nic, new->connect_info.target_addr, &pkt, sizeof(tcp_packet_t));

            TCB_UNLOCK(new);

            break;
        }

        case TCP_STATE_SYN_SENT: {
            if (tcp_pkt->flags == (TCP_RST | TCP_ACK)) {
                LOG(INFO, "Connection was refused.\n");
                tcb->state = TCP_STATE_CLOSED;
                poll_signal(&tcb->event, POLLERR);
                break;
            }

            if (tcp_pkt->flags != (TCP_SYN | TCP_ACK)) {
                LOG(WARN, "Unexpected packet sent to TCP socket in TCP_STATE_SYN_SENT\n");
                tcp_reset(tcb, ntohl(ip_pkt->src_addr), tcp_pkt);
                break;
            }

            if (ntohl(tcp_pkt->ack) != tcb->send_window.sndnxt) {
                LOG(WARN, "SYN/ACK has invalid ACK number\n");
                tcp_reset(tcb, ntohl(ip_pkt->src_addr), tcp_pkt);
                break;
            }

            LOG(DEBUG, "TCP_STATE_SYN_SENT -> TCP_STATE_SYN_RECV\n");
            tcb->state = TCP_STATE_SYN_RECEIVED;

            // Configure parameters
            uint16_t mss;
            uint8_t wscale;
            tcp_parsePacketOptions(tcp_pkt, &wscale, &mss);
            LOG(DEBUG, "Parsed MSS %04x Wscale %x\n", mss, wscale);
            tcb->send_window.sndwnd = ntohs(tcp_pkt->window_size);
            tcb->send_window.sndscale = wscale;
            tcb->send_window.sndwl1 = ntohl(tcp_pkt->seq);
            tcb->send_window.sndwl2 = ntohl(tcp_pkt->ack);
            tcb->send_window.sndmss = min(mss, tcb->send_window.sndmss); // SNDMSS was already setup by tcp_connect as the NIC's MTU
            tcb->recv_window.rcvnxt = tcb->send_window.sndwl1 + 1;
            tcb->recv_window.rcvwnd = TCP_DFL_WINDOW_SIZE;
        
            
            // reply with ACK
            if (tcp_acknowledge(tcb)) {
                LOG(ERR, "Error while ACKing SYN/ACK\n");
                poll_signal(&tcb->event, POLLERR);
                break;
            }

            LOG(DEBUG, "TCP_STATE_SYN_RECV -> TCP_STATE_ESTABLISHED\n");
            tcb->state = TCP_STATE_ESTABLISHED;

            poll_signal(&tcb->event, POLLIN);
            break;
        }

        case TCP_STATE_SYN_RECEIVED: {
            assert(tcp_pkt->flags == TCP_ACK);

            tcb->send_window.sndnxt = ntohl(tcp_pkt->ack);
            tcb->send_window.sndwnd = ntohs(tcp_pkt->window_size);
            tcb->send_window.snduna = ntohl(tcp_pkt->ack);
            
            tcp_tcb_t *parent = tcb->parent;
            tcb->state = TCP_STATE_ESTABLISHED;
            TCB_LOCK(parent);
            
            // Push and alert!
            queue_push(parent->pending_connections, tcb);
            poll_signal(&parent->event, POLLIN);
            TCB_UNLOCK(parent);

            break;
        }

        case TCP_STATE_CLOSED:
            break;

        case TCP_STATE_ESTABLISHED: {
            LOG(DEBUG, "ESTABLISHED socket flags %x.\n", tcp_pkt->flags);
            if (tcp_pkt->flags & TCP_PSH || tcp_pkt->flags & TCP_ACK) {
                // Calculate payload size
                size_t payload_len = ntohs(ip_pkt->length) - sizeof(ipv4_packet_t) - (tcp_pkt->data_offset * 4);
                
                if (payload_len > 0) {
                    char *payload = (char*)tcp_pkt + (tcp_pkt->data_offset * 4);
                    
                    LOG(DEBUG, "Received %d bytes\n", payload_len);
                    TCB_LOCK_RCV(tcb);
                    ssize_t written = ringbuffer_write(tcb->recv_window.buffer, payload, payload_len);
                    tcb->recv_window.rcvwnd -= written;
                    TCB_UNLOCK_RCV(tcb);
                    
                    // Only advance rcvnxt and ACK if we successfully wrote all data
                    if (written == (ssize_t)payload_len) {
                        tcb->recv_window.rcvnxt += payload_len;
                        tcp_acknowledge(tcb);
                        poll_signal(&tcb->wndev, POLLIN);
                    } else {
                        LOG(WARN, "Receive buffer full, discarding %zu bytes\n", payload_len - written);
                        tcp_reset(tcb, ntohl(ip_pkt->src_addr), tcp_pkt);
                    }
                } else {
                    if (tcp_pkt->flags & TCP_ACK) {
                        tcb->send_window.sndwnd = ntohs(tcp_pkt->window_size);
                        tcb->send_window.snduna = ntohl(tcp_pkt->ack);
                    }
                }
            } else if (tcp_pkt->flags & TCP_FIN) {
                // FIN packet, we can transition to CLOSE_WAIT state.
                LOG(INFO, "Got FIN packet, transitioning to CLOSE_WAIT\n");
                tcb->state = TCP_STATE_CLOSE_WAIT;
                tcp_acknowledge(tcb);
            } else {
                LOG(ERR, "set flags: %x\n", tcp_pkt->flags);
                assert(0 && "ESTABLISHED: Unimplemented");
            }


            break;
        }

        
        // TODO: All of these close states are very volatile and need improvements

        case TCP_STATE_FIN_WAIT_1: {
            // FIN-WAIT-1 state, we are looking for an ACK.
            // TODO: TIMEOUTS AND SUPPORTING FIN/FIN-ACK
            assert(tcp_pkt->flags == TCP_ACK); // note that this is actually wrong and it could be in any order. 
            LOG(DEBUG, "Got ACK, going to FIN_WAIT_2\n");
            tcb->state = TCP_STATE_FIN_WAIT_2;

            break;
        }

        case TCP_STATE_FIN_WAIT_2: {
            // FIN-WAIT-2 state
            // we have already sent and gotten a FIN acked again
            if (tcp_pkt->flags == TCP_FIN || tcp_pkt->flags == (TCP_FIN | TCP_ACK)) {
                tcb->recv_window.rcvnxt++;
                tcp_acknowledge(tcb);
                tcb->state = TCP_STATE_TIME_WAIT;

                // TODO: proper TCP_STATE_TIME_WAIT, right now this will release the initial ref we had on it
                TCB_RELEASE(tcb);
            } else {
                LOG(WARN, "Discarding packet (in FIN-WAIT-2 state) with flags %x\n", tcp_pkt->flags);
                size_t payload_len = ntohs(ip_pkt->length) - sizeof(ipv4_packet_t) - (tcp_pkt->data_offset * 4);
                if (payload_len) {
                    // !!! Discard the bytes of the packet
                    tcb->recv_window.rcvnxt += payload_len + ((tcp_pkt->flags & TCP_FIN));
                    tcp_acknowledge(tcb);
                }
            }

            break;
        }

        case TCP_STATE_CLOSE_WAIT: {
            // Waiting for the user to close their socket
            // TODO: what do we do? im just going to ACK whatever comes in
            LOG(DEBUG, "got packet in CLOSE_WAIT\n");
            tcp_acknowledge(tcb);
            break;
        }
        
        case TCP_STATE_LAST_ACK: {
            // If we get a packet in the LAST_ACK state we are good
            assert(tcp_pkt->flags == TCP_ACK);

            // Now we can release the last reference
            TCB_RELEASE(tcb);
            break;
        }


        case TCP_STATE_TIME_WAIT: {
            // Very rare it reaches this since the FIN-WAIT-2 code releases the ref
            LOG(DEBUG, "Got packet (flags = %x) in TIME_WAIT - probably bug\n", tcp_pkt->flags);
            break;
        }
        
        default:
            assert(0 && "Unimplemented TCB state");
    }

    TCB_UNLOCK(tcb);
    TCB_RELEASE(tcb);

}

/**
 * @brief Worker flush send window
 */
void tcp_workerFlushSend(tcp_worker_t *worker, tcp_worker_request_t *req) {
    tcp_tcb_t *tcb = req->tcb;
    TCB_LOCK_SND(tcb);

    tcb->awaiting_flush = false;

    while (ringbuffer_remaining_read(tcb->send_window.buffer) > 0 && tcb->send_window.sndwnd > 0) {
        if (tcp_transmit(tcb) < 0) break;
    }

    TCB_UNLOCK_SND(tcb);
    TCB_RELEASE(tcb);
}

/**
 * @brief TCP worker function
 */
void tcp_worker(void *arg) {
    tcp_worker_t *worker = (tcp_worker_t*)arg;
    for (;;) {
        mutex_acquire(&worker->lock);
        if (queue_rb_empty(&worker->request_queue)) {
            mutex_release(&worker->lock);
            sleep_prepare();
            sleep_enter();
            continue;
        }

        tcp_worker_request_t *request;
        assert(queue_rb_pop(&worker->request_queue, (void**)&request) == 0);
        worker->requests--;

        if (request->request_type == TCP_WORKER_CLOSE) {
            assert(0 && "TODO: TCP_WORKER_CLOSE");
        } else if (request->request_type == TCP_WORKER_PROCESS_PACKET) {
            tcp_workerProcessPacket(worker, request);
        } else if (request->request_type == TCP_WORKER_FLUSH_SEND) {
            tcp_workerFlushSend(worker, request);
        } else {
            assert(0);
        }

        kfree(request);

        mutex_release(&worker->lock);
    }
}

/**
 * @brief TCP init worker
 */
void tcp_initWorker(int num) {
    tcp_worker_t *worker = &tcp_workers[num];
    MUTEX_INIT(&worker->lock);
    QUEUE_RB_INIT(&worker->request_queue, QUEUE_DEFAULT_SIZE);
    worker->requests = 0;
    if (tcp_process) {
        worker->thr = process_createKernelThread(tcp_process, 0, tcp_worker, worker);
        scheduler_insertThread(worker->thr);
    }
}

/**
 * @brief TCP packet handler
 */
int tcp_handle(nic_t *nic, void *packet, size_t size) {
    ipv4_packet_t *ip_packet = (ipv4_packet_t*)packet;
    tcp_packet_t *tcp_pkt = (tcp_packet_t*)ip_packet->payload;

    // make sure a port is registered
    tcp_tcb_t *tcb = tcp_findTCB(ntohs(tcp_pkt->dst_port), ntohl(ip_packet->src_addr), ntohs(tcp_pkt->src_port));
    if (!tcb) {
        // NOTE: It doesn't matter if we didnt find a TCB as the worker will send back an RST
        LOG(WARN, "TCP: Could not find handler for port %d\n", ntohs(tcp_pkt->dst_port));
    }

    // find a worker thread to process this
    tcp_worker_t *worker = tcp_findWorker(TCP_WORKER_PROCESS_PACKET);
    assert(worker); // TODO
    tcp_requestWorker(worker, tcb, TCP_WORKER_PROCESS_PACKET, packet, size);
    tcp_finishWorker(worker);

    // findTCB returns a tcb with a ref on it
    if (tcb) TCB_RELEASE(tcb);

    return 0;
}

/**
 * @brief Init a TCB
 * @param tcb The TCB to init
 */
void tcp_initTCB(tcp_tcb_t *tcb) {
    // Initialize the TCB
    // TODO maybe do the TCB initialization in connect/listen so that memory isnt wasted
    MUTEX_INIT(&tcb->lck);
    POLL_EVENT_INIT(&tcb->event);
    POLL_EVENT_INIT(&tcb->wndev);
    tcb->transmit_buffer = NULL;
    refcount_init(&tcb->ref, 1);
    
    // initialize recv_window
    tcb->recv_window.buffer = ringbuffer_create(TCP_DFL_WINDOW_SIZE);
    tcb->recv_window.rcvwnd = TCP_DFL_WINDOW_SHIFT;
    MUTEX_INIT(&tcb->recv_window.rcvlck);

    // initialize send window
    tcb->send_window.buffer = ringbuffer_create(TCP_DFL_WINDOW_SIZE);
    MUTEX_INIT(&tcb->send_window.sndlck);

    // temporary, until socket does anything
    tcb->state = TCP_STATE_CLOSED;
}


/**
 * @brief Locate a TCB for a port
 * @param port The port to locate the TCB for
 * @param remote_host The remote host to locate
 * @param remote_port The remote port to locate
 * @returns A TCB with an extra reference or NULL
 */
tcp_tcb_t *tcp_findTCB(in_port_t port, in_addr_t remote_host, in_port_t remote_port) {
    mutex_acquire(&tcp_port_map_mutex);
    tcp_tcb_t *tcb = TCP_TCB(hashmap_get(tcp_port_map, (void*)(uintptr_t)port));
    mutex_release(&tcp_port_map_mutex);

    // Ok we have a TCB but does it have a subconnection?

    if (tcb && tcb->sub_link) {
        // Yes, let's find it
        TCB_LOCK(tcb);
        
        tcp_tcb_t *iter = tcb->sub_link;
        while (iter) {
            if (iter->connect_info.target_addr == remote_host && iter->connect_info.target_port == remote_port) {
                assert(iter->port == port);
                TCB_HOLD(iter);
                TCB_UNLOCK(tcb);
                return iter;
            }

            iter = iter->sub_link;
        }

        TCB_UNLOCK(tcb);
    }

    if (tcb) TCB_HOLD(tcb);
    return tcb;
}

/**
 * @brief Free the TCB
 */
static void tcp_freeTCB(tcp_tcb_t *tcb) {
    LOG(ERR, "TODO: Free TCB\n");
}

/**
 * @brief TCB setup port
 */
static int tcp_initTCBPort(tcp_tcb_t *tcb, uint16_t port) {
    spinlock_acquire(&tcp_port_lock);
    if (!port) {
        // allocate a port
        port = TCP_DEFAULT_PORT;
        while (TCP_PORT_GET(port)) port++;
    }

    if (TCP_PORT_GET(port)) {
        spinlock_release(&tcp_port_lock);
        return -EADDRINUSE;
    }

    TCP_PORT_SET(port, 1);
    tcb->port = port;
    spinlock_release(&tcp_port_lock);

    // TODO: racey?
    mutex_acquire(&tcp_port_map_mutex);
    hashmap_set(tcp_port_map, (void*)(uintptr_t)port, tcb);
    mutex_release(&tcp_port_map_mutex);

    return 0;
}


/**
 * @brief TCP bind
 */
static int tcp_bind(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    if (addrlen < sizeof(struct sockaddr_in)) return -EINVAL;

    TCB_LOCK(tcb);

    if (tcb->port || tcb->state != TCP_STATE_CLOSED) {
        // This TCB is already bound or not ready to bind
        TCB_UNLOCK(tcb);
        return -EINVAL;
    }
    
    struct sockaddr_in *addr = (struct sockaddr_in*)sockaddr;
    uint16_t target_port = ntohs(addr->sin_port);
    
    int status = tcp_initTCBPort(tcb, target_port);

    TCB_UNLOCK(tcb);
    return status;
}

/**
 * @brief TCP connect
 */
static int tcp_connect(sock_t *sock, const struct sockaddr *sockaddr, socklen_t addrlen) {
    if (addrlen < sizeof(struct sockaddr_in)) return -EINVAL;
    struct sockaddr_in *addr = (struct sockaddr_in*)sockaddr;
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);

    // Route a NIC
    nic_t *nic = nic_route(ntohl(addr->sin_addr.s_addr));
    if (!nic) return -EHOSTUNREACH;

    // Bind to a port if not bound yet
    if (!tcb->port) {
        int s = tcp_initTCBPort(tcb, 0);
        if (s != 0) return s;
    }

    // Lock the TCB
    LOG(DEBUG, "Begin connection\n");
    TCB_LOCK(tcb);

    // We must be in closed state to connect
    if (tcb->state != TCP_STATE_CLOSED) {
        TCB_UNLOCK(tcb);
        return -EISCONN;
    }


    // Configure connection info
    tcb->connect_info.target_addr = ntohl(addr->sin_addr.s_addr);
    tcb->connect_info.target_port = ntohs(addr->sin_port);
    
    LOG(DEBUG, "Doing connection %d -> %d\n", tcb->port, tcb->connect_info.target_port);

    uint32_t iss = rand();

    // Setup the sending window
    mutex_acquire(&tcb->send_window.sndlck);
    tcb->send_window.sndmss = nic->mtu - sizeof(ipv4_packet_t) - sizeof(tcp_packet_t);
    tcb->send_window.sndnxt = iss;
    tcb->send_window.snduna = iss;
    mutex_release(&tcb->send_window.sndlck);


    if (!tcb->transmit_buffer) {
        // TODO: Allocate this dependent on the remote host's MSS (min(rhost_mss, tcb->send_window.sndmss))
        tcb->transmit_buffer = kmalloc(tcb->send_window.sndmss + sizeof(tcp_packet_t));
    }

    // Prepare and attach listener
    poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, 1);
    poll_add(waiter, &tcb->event, POLLIN | POLLERR);

    // Include window scaling options
    // TODO: implement MSS
    char syn_data[sizeof(tcp_packet_t) + 4];
    tcp_packet_t *syn_pkt = (tcp_packet_t*)syn_data;
    tcp_initPacket(syn_pkt, tcb, 0, TCP_SYN);
    syn_pkt->data_offset = ((sizeof(tcp_packet_t) + 4) / 4);

    syn_data[sizeof(tcp_packet_t)] = 3; // WSopt
    syn_data[sizeof(tcp_packet_t)+1] = 3; // 3 bytes of length
    syn_data[sizeof(tcp_packet_t)+2] = TCP_DFL_WINDOW_SHIFT;
    syn_data[sizeof(tcp_packet_t)+3] = 0; // End

    // Now send the packet
    int error = tcp_sendPacket(tcb, nic, tcb->connect_info.target_addr, (tcp_packet_t*)syn_data, sizeof(syn_data));
    if (error) {
        TCB_UNLOCK(tcb);
        LOG(WARN, "An error occurred while sending SYN: %d\n", error);
        return error;
    }

    // Advance to next packet
    tcb->send_window.sndnxt++;
    tcb->state = TCP_STATE_SYN_SENT;

    TCB_UNLOCK(tcb);
    int status = poll_wait(waiter, 3000);

    poll_exit(waiter);
    poll_destroyWaiter(waiter);

    if (status != 0) {
        LOG(ERR, "Connection failed with error %d\n", status);
        return status;
    }
    
    // We should have transitioned to the ESTABLISHED state
    if (tcb->state != TCP_STATE_ESTABLISHED) {
        // TODO: Use poll results, this is unreliable
        LOG(ERR, "Socket did not transition into TCP_STATE_ESTABLISHED\n");
        return -ECONNREFUSED;
    }

    LOG(DEBUG, "Connection succeeded.\n");
    return 0;
}

/**
 * @brief TCP sendmsg
 */
static ssize_t tcp_sendmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    assert(msg->msg_flags == 0);

    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    TCB_HOLD(tcb);

    if (tcb->state != TCP_STATE_ESTABLISHED) {
        TCB_RELEASE(tcb);

        // TODO: ECONNRESET
        return -ENOTCONN;
    }

    // If the ringbuffer is full init a listener
    TCB_LOCK_SND(tcb);
    while (ringbuffer_remaining_write(tcb->send_window.buffer) == 0) {
        LOG(DEBUG, "Waiting for TCB to be ready to send...\n");
        poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(waiter, &tcb->wndev, POLLOUT | POLLERR);
        TCB_UNLOCK_SND(tcb);
        int w = poll_wait(waiter, -1);
     
        poll_exit(waiter);
        poll_destroyWaiter(waiter);

        if (w) {
            TCB_RELEASE(tcb);
            return w;
        }

        TCB_LOCK_SND(tcb);
    }

    // The TCB is locked and the send window is ready, write as much as possible to it
    ssize_t written = 0;
    size_t remaining = ringbuffer_remaining_write(tcb->send_window.buffer);
    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        if (!remaining) break;
        struct iovec *iov = &msg->msg_iov[i];
        ssize_t to_send = min(iov->iov_len, remaining);
        ringbuffer_write(tcb->send_window.buffer, (char*)iov->iov_base, to_send);
        remaining -= to_send;
        written += to_send;
    }

    LOG(DEBUG, "Wrote %d bytes to TCB need_flush=%d\n", written, tcb->awaiting_flush);
    // If needed send a request
    if (!tcb->awaiting_flush) {
        tcp_worker_t *worker = tcp_findWorker(TCP_WORKER_FLUSH_SEND);
        assert(worker);
        tcp_requestWorker(worker, tcb, TCP_WORKER_FLUSH_SEND, NULL, 0);
        tcp_finishWorker(worker);
        tcb->awaiting_flush = true;
    }

    TCB_UNLOCK_SND(tcb);
    TCB_RELEASE(tcb);

    return written;

}

/**
 * @brief TCP recvmsg
 */
static ssize_t tcp_recvmsg(sock_t *sock, struct msghdr *msg, int flags) {
    if (!msg->msg_iovlen) return 0;
    assert(msg->msg_flags == 0);

    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    TCB_HOLD(tcb);

    if (tcb->state != TCP_STATE_ESTABLISHED) {
        TCB_RELEASE(tcb);

        // TODO: ECONNRESET
        return -ENOTCONN;
    }

    
    // If the ringbuffer is empty init a listener
    TCB_LOCK_RCV(tcb);
    while (ringbuffer_remaining_read(tcb->recv_window.buffer) == 0) {
        poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(waiter, &tcb->wndev, POLLIN | POLLERR);
        TCB_UNLOCK_RCV(tcb);
        int w = poll_wait(waiter, -1);
     
        poll_exit(waiter);
        poll_destroyWaiter(waiter);

        if (w) {
            TCB_RELEASE(tcb);
            return w;
        }

        TCB_LOCK_RCV(tcb);
    }

    // RCV is locked and ready to go
    ssize_t read = 0;
    size_t remaining = ringbuffer_remaining_read(tcb->recv_window.buffer);
    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        if (!remaining) break;
        struct iovec *iov = &msg->msg_iov[i];
        ssize_t to_read = min(iov->iov_len, remaining);
        to_read = ringbuffer_read(tcb->recv_window.buffer, (char*)iov->iov_base, to_read);
        tcb->recv_window.rcvwnd += to_read;
        remaining -= to_read;
        read += to_read;
    }


    TCB_UNLOCK_RCV(tcb);
    TCB_RELEASE(tcb);
    return read;
}

/**
 * @brief TCP poll events
 */
static poll_events_t tcp_poll_events(sock_t *sock) {
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);;
    poll_events_t events = 0;

    switch (tcb->state) {
        case TCP_STATE_LISTEN:
            events |= (queue_empty(tcb->pending_connections) ? 0 : POLLIN);
            break;
            
        case TCP_STATE_ESTABLISHED:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECEIVED:
            // Check if data available
            events |= (ringbuffer_remaining_write(tcb->send_window.buffer) ? POLLOUT : 0) | (ringbuffer_remaining_read(tcb->recv_window.buffer) ? POLLIN : 0);
            break;
        
        default:
            // The rest of these are closed
            events |= POLLHUP | (ringbuffer_remaining_read(tcb->recv_window.buffer));
            break;
    }

    return events;
}

/**
 * @brief TCP poll
 */
static int tcp_poll(sock_t *sock, poll_waiter_t *waiter, poll_events_t events) {
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    
    switch (tcb->state) {
        case TCP_STATE_ESTABLISHED:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECEIVED:
            // Connection is still in progress
            poll_add(waiter, &tcb->wndev, events);
            break;

        default:
            poll_add(waiter, &tcb->event, events);
            break;
    }

    return 0;
}


/**
 * @brief TCP close
 */
static int tcp_close(sock_t *sock) {
    // TODO: Kernel timer interface and then we can actually get a good fucking timeout system. Right now about anything can DoS the kernel.

    // There are like 4 possibilites to this:
    // - We are established (or CLOSE_WAIT), send FIN-ACK and transition state
    // - We are a listening socket, so flush backlog
    // - We are SYN_SENT meaning the client has not yet accepted our connection request
    // - We are already closed

    
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    
    // If this TCB had a parent, remove the TCB from its children list
    if (tcb->parent) {
        TCB_LOCK(tcb->parent);

        tcp_tcb_t *sub = tcb->parent->sub_link;

        if (sub == tcb) {
            tcb->parent->sub_link = sub->sub_link;
        } else {
            while (sub->sub_link != tcb) sub = sub->sub_link;
            if (sub) {
                sub->sub_link = tcb->sub_link; 
            }
        }

        TCB_UNLOCK(tcb->parent);
    }

    TCB_LOCK(tcb);
    if (tcb->state == TCP_STATE_ESTABLISHED || tcb->state == TCP_STATE_CLOSE_WAIT) {
        // First flush transmit queue
        TCB_LOCK_SND(tcb);
        
        tcb->awaiting_flush = false;

        while (ringbuffer_remaining_read(tcb->send_window.buffer) > 0 && tcb->send_window.sndwnd > 0) {
            if (tcp_transmit(tcb) < 0) break;
        }
        TCB_UNLOCK_SND(tcb);

        tcb->state = (tcb->state == TCP_STATE_ESTABLISHED) ? TCP_STATE_FIN_WAIT_1 : TCP_STATE_LAST_ACK;

        nic_t *nic = nic_route(tcb->connect_info.target_addr);

        // FIN-ACK 
        tcp_packet_t pkt;
        tcp_initPacket(&pkt, tcb, 0, TCP_FIN | TCP_ACK);
        tcp_sendPacket(tcb, nic, tcb->connect_info.target_addr, &pkt, sizeof(tcp_packet_t));
        tcb->send_window.sndnxt++;

        // Do not release the initial ref just yet but wait for the state machine to do that
    } else if (tcb->state == TCP_STATE_LISTEN) {
        // There really isnt much we can do here since the child TCBs rely on this guy.
        // Set to closed to stop new connections then release the initial ref
        dprintf(DEBUG, "The listener has CLOSED\n");
        tcb->state = TCP_STATE_CLOSED;
        TCB_RELEASE(tcb);
    } else if (tcb->state == TCP_STATE_SYN_SENT) {
        assert(0 && "TODO: close TCP_STATE_SYN_SENT");
    } else {
        LOG(DEBUG, "Attempted close in invalid state %d\n", tcb->state);
    }

    TCB_UNLOCK(tcb);
    
    return 0;
}

/**
 * @brief TCP listen
 */
static int tcp_listen(sock_t *sock, int backlog) {
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    if (tcb->state != TCP_STATE_CLOSED) {
        return -EINVAL;
    }

    TCB_LOCK(tcb);
    tcb->state = TCP_STATE_LISTEN;
    tcb->pending_connections = queue_create();
    tcb->parent = NULL;
    tcb->sub_link = NULL;
    TCB_UNLOCK(tcb);

    return 0;
}

/**
 * @brief TCP accept
 */
static int tcp_accept(struct sock *sock, struct sockaddr *sockaddr, socklen_t *addrlen) {
    tcp_tcb_t *tcb = TCP_TCB(sock->driver);
    if (tcb->state != TCP_STATE_LISTEN) {
        return -EINVAL;
    }
    
    TCB_LOCK(tcb);

    while (queue_empty(tcb->pending_connections)) {
        if (sock_nonblocking(sock)) {
            TCB_UNLOCK(tcb);
            return -EAGAIN;
        }

        poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(waiter, &tcb->event, POLLIN);
        TCB_UNLOCK(tcb);

        int w = poll_wait(waiter, -1);
        poll_exit(waiter);
        poll_destroyWaiter(waiter);

        if (w) return w;

        TCB_LOCK(tcb); 
    }

    // pop the latest connection from queue
    tcp_tcb_t *new = queue_pop(tcb->pending_connections);
    assert(new);

    sock_t *sck = socket_allocate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sck->ops = &tcp_socket_ops;
    sck->driver = (void*)new;

    LOG(DEBUG, "Accepted connection\n");

    TCB_UNLOCK(tcb);

    if (addrlen) {
        size_t size = (*addrlen > sizeof(struct sockaddr_in)) ? sizeof(struct sockaddr_in) : *addrlen;
        SYSCALL_VALIDATE_PTR_SIZE(sockaddr, *addrlen);
    
        struct sockaddr_in in;
        in.sin_family = AF_INET;
        in.sin_port = htons(new->connect_info.target_port);
        in.sin_addr.s_addr = htonl(new->connect_info.target_addr);
        memcpy(sockaddr, &in, size);
    }

    int sfd = socket_insert(sck);
    return sfd;
}

/**
 * @brief Create a TCP socket
 */
sock_t *tcp_socket() {
    sock_t *sock = socket_allocate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
 
    tcp_tcb_t *tcb = slab_allocate(tcb_slab);
    memset(tcb, 0, sizeof(tcp_tcb_t));

    tcp_initTCB(tcb);

    sock->ops = &tcp_socket_ops;
    sock->driver = tcb;

    return sock;
}


/**
 * @brief TCP init routine
 */
static int tcp_init() {
    tcb_slab = slab_createCache("tcp tcb", SLAB_CACHE_DEFAULT, sizeof(tcp_tcb_t), 0, NULL, NULL);
    tcp_port_map = hashmap_create_int("tcp port map", 200);
    memset(tcp_port_bitmap, 0, sizeof(tcp_port_bitmap));

    return ipv4_register(IPV4_PROTOCOL_TCP, tcp_handle);
}

/**
 * @brief TCP init workers
 */
static int tcp_init_workers() {
    // abuse the process system
    tcp_initWorker(0);
    tcp_process = process_createKernel("tcp worker", PROCESS_KERNEL, PRIORITY_HIGH, tcp_worker, &tcp_workers[0]);
    tcp_workers[0].thr = tcp_process->main_thread;
    scheduler_insertThread(tcp_process->main_thread);

    for (int i = 1; i < TCP_THREADS; i++) {
        tcp_initWorker(i);
    }

    return 0;
}

NET_INIT_ROUTINE(tcp, INIT_FLAG_DEFAULT, tcp_init, ipv4);
SCHED_INIT_ROUTINE(tcp_workers, INIT_FLAG_DEFAULT, tcp_init_workers);
