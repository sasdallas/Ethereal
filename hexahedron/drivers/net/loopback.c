/**
 * @file hexahedron/drivers/net/loopback.c
 * @brief Loopback interface
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/net/route.h>
#include <kernel/init.h>
#include <arpa/inet.h>

static ssize_t loopback_send(nic_t *n, size_t size, char *buffer);
static nic_ops_t loopback_nic_ops = {
    .send = loopback_send,
    .ioctl = NULL
};

/**
 * @brief Loopback send
 */
static ssize_t loopback_send(nic_t *n, size_t size, char *buffer) {
    // Loop right back atcha
    n->stats.rx_bytes += size;
    n->stats.rx_packets++;
    n->stats.tx_bytes += size;
    n->stats.tx_packets++;
    ethernet_handle((ethernet_packet_t*)buffer, n, size);
    return size;
}

/**
 * @brief Install the loopback device
 */
static int loopback_install() {
    uint8_t mac[6] = {00, 00, 00, 00, 00, 00};
    nic_t *nic = nic_create("lo", NIC_TYPE_ETHERNET, &loopback_nic_ops, mac, NULL);

    nic->ipv4_address = 0x7F000001; // 127.0.0.1
    nic->ipv4_gateway = 0x00000000; // 0.0.0.0
    nic->ipv4_subnet =  0x000000FF; // 255.0.0.0
    nic->mtu = 1500;
    
    // Add a route for localhost
    route_ipv4_t loopback_route = {
        .dev = nic,
        .dest = 0x7F000001,
        .gw = 0x00000000,
        .netmask = 0xFF000000,
        .metric = 10000,
        .flags = RT_FLAG_UP
    };

    route_add(&loopback_route);

    return 0;
}

NET_INIT_ROUTINE(loopback, INIT_FLAG_DEFAULT, loopback_install, route);
