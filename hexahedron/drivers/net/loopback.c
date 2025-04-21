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

#include <kernel/drivers/net/loopback.h>
#include <kernel/drivers/net/ethernet.h>
#include <arpa/inet.h>

/**
 * @brief Loopback write
 */
ssize_t loopback_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    // Loop right back atcha
    ethernet_handle((ethernet_packet_t*)buffer, node, size);
    return size;
}

/**
 * @brief Install the loopback device
 */
void loopback_install() {
    uint8_t mac[6] = {00, 00, 00, 00, 00, 00};
    fs_node_t *nic_node = nic_create("loopback interface", mac, NIC_TYPE_ETHERNET, NULL);

    NIC(nic_node)->ipv4_address = inet_addr("127.0.0.1");
    NIC(nic_node)->ipv4_subnet =  inet_addr("255.0.0.0");

    nic_node->write = loopback_write;
    nic_register(nic_node, "lo");
}