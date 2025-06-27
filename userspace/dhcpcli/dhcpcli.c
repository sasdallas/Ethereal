/**
 * @file userspace/dhcpcli/dhcpcli.c
 * @brief Ethereal DHCP client
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "dhcpcli.h"
#include <sys/socket.h>
#include <stdio.h>
#include <getopt.h>
#include <kernel/drivers/nicdev.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>

/* Verbose mode enabled */
static int verbose = 0;

/* Transaction ID */
static uint32_t xid = 0xDEADBEEF;

/* Log method */
#define DHCP_LOG(...) { if (verbose) printf("dhcpcli: " __VA_ARGS__); }
#define DHCP_ERR(...) { fprintf(stderr, __VA_ARGS__); printf("DHCP error: " __VA_ARGS__); }

/* Push unto options */
#define DHCP_OPTION_START() size_t optidx = 0
#define DHCP_OPTION_PUSH(opt) packet.options[optidx++] = opt
#define DHCP_OPTION_PUSH_SINGLE(opt, val) { DHCP_OPTION_PUSH(opt); DHCP_OPTION_PUSH(1); DHCP_OPTION_PUSH(val); }
#define DHCP_OPTION_END()  { DHCP_OPTION_PUSH(DHCP_OPT_END); }

#define STR(x) #x

#define DHCP_OPTION_POP_START() DHCP_OPTION_START()
#define DHCP_OPTION_TYPE() pkt->options[optidx]
#define DHCP_OPTION_SIZE() pkt->options[optidx+1]
#define DHCP_OPTION_VALUE() &(pkt->options[optidx+2])
#define DHCP_OPTION_NEXT() { optidx += 2 + DHCP_OPTION_SIZE(); }
#define DHCP_OPTION_NEED_LEN(opt, len) { if (DHCP_OPTION_SIZE() != len) { DHCP_ERR("Invalid length for opt " opt ": %d\n", DHCP_OPTION_SIZE()); return 1; }}

/**
 * @brief Send a DHCP packet
 * @param sock The socket to use when sending the DHCP packet
 * @param server The server to send to
 * @param dhcp_packet The packet to send
 * @returns 0 on success
 */
int dhcp_send(int sock, in_addr_t server, dhcp_packet_t *dhcp_packet) {
    struct sockaddr_in serv;
    serv.sin_addr.s_addr = server;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(67);

    if (sendto(sock, (void*)dhcp_packet, sizeof(dhcp_packet_t), 0, (const struct sockaddr*)&serv, sizeof(struct sockaddr_in)) < 0) {
        perror("sendto");
        return 1;
    }

    return 0;
}

/**
 * @brief Send a DHCPDISCOVER
 * @param sock The socket to send the discover on
 * @param mac The MAC address
 */
int dhcp_discover(int sock, uint8_t *mac) {
    dhcp_packet_t packet;

    packet.opcode = DHCP_OP_REQUEST;
    packet.htype = DHCP_HTYPE_ETH;
    packet.hlen = 6;
    packet.hops = 0;
    packet.xid = xid;
    packet.secs = 0;
    packet.flags = 0;
    memcpy(packet.chaddr, mac, 6);
    packet.magic = htonl(DHCP_MAGIC);

    DHCP_OPTION_START();
    DHCP_OPTION_PUSH_SINGLE(DHCP_OPT_MESSAGE_TYPE, DHCPDISCOVER); // DHCPDISCOVER
    DHCP_OPTION_PUSH(DHCP_OPT_PARAMETER_REQ);   // Request parameters
    DHCP_OPTION_PUSH(0x03);                     // 3 parameters
    DHCP_OPTION_PUSH(DHCP_OPT_DNS);             // DNS
    DHCP_OPTION_PUSH(DHCP_OPT_SUBNET_MASK);     // Subnet mask
    DHCP_OPTION_PUSH(DHCP_OPT_ROUTER);          // Router
    DHCP_OPTION_END();

    DHCP_LOG("sending DHCPDISCOVER to all addresses (DNS SUBNET ROUTER)\n");
    return dhcp_send(sock, 0xFFFFFFFF, &packet);
}

/**
 * @brief Send DHCPREQUEST
 * @param sock The socket to send to
 * @param mac The MAC client
 * @param req_ip The given IP address
 * @param server_ip The server IP address
 */
int dhcp_request(int sock, uint8_t *mac, in_addr_t req_ip, in_addr_t server_ip) {
    dhcp_packet_t packet;

    packet.opcode = DHCP_OP_REQUEST;
    packet.htype = DHCP_HTYPE_ETH;
    packet.hlen = 6;
    packet.hops = 0;
    packet.xid = xid;
    packet.secs = 0;
    packet.flags = 0;
    memcpy(packet.chaddr, mac, 6);
    packet.magic = htonl(DHCP_MAGIC);
    packet.siaddr = server_ip;

    // Build options
    DHCP_OPTION_START();
    DHCP_OPTION_PUSH_SINGLE(DHCP_OPT_MESSAGE_TYPE, DHCPREQUEST); // DHCPREQUEST
    DHCP_OPTION_PUSH(DHCP_OPT_REQUESTED_IP);
    DHCP_OPTION_PUSH(0x4);
    *(in_addr_t*)(&packet.options[optidx]) = req_ip;
    optidx += sizeof(in_addr_t);
    DHCP_OPTION_PUSH(DHCP_OPT_END);

    DHCP_LOG("sending DHCPREQUEST to server %s ", inet_ntoa((struct in_addr){ .s_addr = server_ip }));
    DHCP_LOG("for IP %s\n", inet_ntoa((struct in_addr){ .s_addr = req_ip }));
    return dhcp_send(sock, server_ip, &packet);
}

/**
 * @brief Parse DHCP options
 * @param pkt The packet to parse on
 * @param opt The options to parse
 */
int dhcp_parse(dhcp_packet_t *pkt, dhcp_options_t *opt) {

    DHCP_OPTION_POP_START();
    while (optidx < 256) {
        if (DHCP_OPTION_TYPE() == DHCP_OPT_PADDING) continue;
        if (DHCP_OPTION_TYPE() == DHCP_OPT_END) return 0;
    
        switch (DHCP_OPTION_TYPE()) {
            case DHCP_OPT_MESSAGE_TYPE:
                DHCP_LOG("option: msg type\n");
                DHCP_OPTION_NEED_LEN(STR(DHCP_OPT_MESSAGE_TYPE), 1);
                opt->msg_type = *DHCP_OPTION_VALUE();
                break;
            case DHCP_OPT_SUBNET_MASK:
                // Subnet mask
                DHCP_LOG("option: subnet mask\n");
                DHCP_OPTION_NEED_LEN(STR(DHCP_OPT_SUBNET_MASK), 4);
                opt->subnet_mask = *(uint32_t*)DHCP_OPTION_VALUE();
                break;
            case DHCP_OPT_ROUTER:
                // Router
                DHCP_LOG("option: router\n");
                opt->router_start = (in_addr_t*)DHCP_OPTION_VALUE();
                if (DHCP_OPTION_SIZE() % 4) {
                    DHCP_ERR("Invalid length for DHCP_OPT_ROUTER: %d\n", DHCP_OPTION_SIZE());
                    return 1;
                }
                opt->router_end = (in_addr_t*)(DHCP_OPTION_VALUE() + DHCP_OPTION_SIZE());
                break;
            case DHCP_OPT_SERVER_ID:
                // Server ID    
                DHCP_LOG("option: server ID\n");
                DHCP_OPTION_NEED_LEN(STR(DHCP_OPT_SERVER_ID), 4);
                opt->server_addr = *(in_addr_t*)DHCP_OPTION_VALUE();
                break;
            case DHCP_OPT_LEASE_TIME:
                // Lease time
                DHCP_LOG("option: lease time\n");
                DHCP_OPTION_NEED_LEN(STR(DHCP_OPT_LEASE_TIME), 4);
                opt->lease_time = *(uint32_t*)DHCP_OPTION_VALUE();
                break;
            case DHCP_OPT_DNS:
                // DNS server
                DHCP_LOG("option: dns server\n");
                opt->dns_start = (in_addr_t*)DHCP_OPTION_VALUE();
                if (DHCP_OPTION_SIZE() % 4) {
                    DHCP_ERR("Invalid length for DHCP_OPT_DNS: %d\n", DHCP_OPTION_SIZE());
                    return 1;
                }
                opt->dns_end = (in_addr_t*)(DHCP_OPTION_VALUE() + DHCP_OPTION_SIZE());
                break;
            default:
                DHCP_LOG("unrecognized option: %d\n", DHCP_OPTION_TYPE());
                break;
        }

        DHCP_OPTION_NEXT();
    }

    return 1;
}

/**
 * @brief Receive a DHCP packet
 * @param sock The socket to receive on
 * @param outpkt The output packet
 * @param optout Output DHCP options
 * @returns 0 on success
 */
int dhcp_receive(int sock, dhcp_packet_t *outpkt, dhcp_options_t *outopt) {
    // TODO: Better timeout?
    struct pollfd fds[1];
    fds[0].fd = sock;
    fds[0].events = POLLIN;

    int p = poll(fds, 1, 10000);
    if (p < 0) {
        perror("poll");
        return 1;
    }

    if (!p) {
        printf("DHCP timeout while receiving\n");
        return 1;
    }

    // Receive the packet
    ssize_t r = recv(sock, (void*)outpkt, sizeof(dhcp_packet_t), 0);
    if (r <= 0) {
        perror("recv");
        return 1;
    }

    if ((size_t)r < sizeof(dhcp_packet_t) - 256) {
        printf("Invalid DHCP offer received (bad length %d expected at least %d)\n", (size_t)r, sizeof(dhcp_packet_t) - 256);
        return 1;
    }

    DHCP_LOG("opcode: %x htype: %x hlen: %d hops: %d xid: %04x\n", outpkt->opcode, outpkt->htype, outpkt->hlen, outpkt->hops, outpkt->xid);
    DHCP_LOG("bootp flags: %04x ciaddr: %04x yiaddr: %04x siaddr: %04x giaddr: %04x\n", outpkt->flags, outpkt->ciaddr, outpkt->yiaddr, outpkt->siaddr, outpkt->giaddr);


    if (outpkt->magic != htonl(DHCP_MAGIC)) {
        printf("Invalid DHCP offer received (bad magic 0x%x expected 0x%x)\n", outpkt->magic, htonl(DHCP_MAGIC));
        return 1;
    }

    if (outpkt->xid != xid) {
        printf("DHCP response is for a different transaction\n");
        return 1;
    }

    return dhcp_parse(outpkt, outopt);
}

/**
 * @brief Usage
 */
void usage() {
    printf("dhcpcli: usage: dhcpcli [-v] [INTERFACE]\n");
    exit(EXIT_FAILURE);
}

/**
 * @brief Version
 */
void version() {
    printf("dhcpcli 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[]) {
    int c;
    while ((c = getopt(argc, argv, "vh")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }

    if (argc - optind < 1) usage();

    DHCP_LOG("dhcpcli 1.0.0\n");
    DHCP_LOG("configuring NIC %s\n", argv[optind]);

    // Get NIC
    char nic_path[512];
    snprintf(nic_path, 512, "/device/%s", argv[optind]);
    int nic = open(nic_path, O_RDONLY);
    if (nic < 0) {
        perror("open");
        return 1;
    }

    // Get the info of the NIC
    nic_info_t info;
    if (ioctl(nic, IO_NIC_GET_INFO, &info) < 0) {
        perror("IO_NIC_GET_INFO");
        close(nic);
        return 1;
    }

    DHCP_LOG("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", info.nic_mac[0], info.nic_mac[1], info.nic_mac[2], info.nic_mac[3], info.nic_mac[4], info.nic_mac[5]);

    // Configure a socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("sock");
        close(nic);
        return 1;
    }

    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(68);
    in.sin_addr.s_addr = 0; // Accept any

    // Bind
    if (bind(sock, (const struct sockaddr*)&in, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        goto _cleanup;
    }

    DHCP_LOG("bound to port 68 completed successfully\n");

    // Randomly generate a transaction ID
    srand(now());
    xid = rand();
    DHCP_LOG("transaction ID: 0x%x\n", xid);

    // Send DISCOVER
    if (dhcp_discover(sock, info.nic_mac)) {
        DHCP_ERR("Failed to send discover request");
        goto _cleanup;
    }

    // Read packet response
    dhcp_packet_t pkt;
    dhcp_options_t opt;
    if (dhcp_receive(sock, &pkt, &opt)) {
        DHCP_ERR("Failed to receieve DHCPOFFER\n");
        goto _cleanup;
    }

    // Validate the response was what we wanted
    if (opt.msg_type != DHCPOFFER) {
        DHCP_ERR("Response is not of type DHCPOFFER\n");
        goto _cleanup;
    }

    DHCP_LOG("DHCPOFFER received:\n");

    struct in_addr i;
    i.s_addr = pkt.yiaddr;
    DHCP_LOG("IP address offered:\t%s\n", inet_ntoa(i));
    i.s_addr = opt.server_addr;
    DHCP_LOG("DHCP server serving:\t%s\n", inet_ntoa(i));
    i.s_addr = opt.subnet_mask;
    DHCP_LOG("Subnet mask offered:\t%s\n", inet_ntoa(i));
    i.s_addr = *opt.router_start;
    DHCP_LOG("Router #1 offered:\t%s\n", inet_ntoa(i));

    in_addr_t offered_addr = pkt.yiaddr;

    // Send REQUEST
    if (dhcp_request(sock, info.nic_mac, pkt.yiaddr, opt.server_addr)) {
        DHCP_ERR("Failed to send DHCPREQUEST\n");
        goto _cleanup;
    }

    // Receive DHCPACK
    if (dhcp_receive(sock, &pkt, &opt)) {
        DHCP_ERR("Failed to retrieve DHCPACK\n");
        goto _cleanup;
    }

    // Validate the response was what we wanted
    if (opt.msg_type != DHCPACK) {
        DHCP_ERR("Response was not of type DHCPACK\n");
        goto _cleanup;
    }

    // Setup parameters
    DHCP_LOG("DHCPACK received:\n");   
    i.s_addr = pkt.yiaddr;
    DHCP_LOG("IP address accepted:\t%s\n", inet_ntoa(i));
    i.s_addr = opt.server_addr;
    DHCP_LOG("DHCP server accepted:\t%s\n", inet_ntoa(i));
    i.s_addr = opt.subnet_mask;
    DHCP_LOG("Subnet mask accepted:\t%s\n", inet_ntoa(i));
    i.s_addr = *opt.router_start;
    DHCP_LOG("Router #1 accepted:\t%s\n", inet_ntoa(i));

    if (pkt.yiaddr != offered_addr) {
        DHCP_ERR("DHCPACK reported IP that was different from one offered\n");
        goto _cleanup;
    }

    // Update NIC info
    info.nic_ipv4_addr = pkt.yiaddr;
    info.nic_ipv4_gateway = *opt.router_start;
    info.nic_ipv4_subnet = opt.subnet_mask;
    
    if (ioctl(nic, IO_NIC_SET_INFO, &info) < 0) {
        perror("IO_NIC_SET_INFO");
        goto _cleanup;
    }

    close(nic);
    close(sock);
    return 0;

_cleanup:
    close(nic);
    close(sock);
    return 1;
}