/**
 * @file userspace/dhcpcli/dhcpcli.h
 * @brief Dynamic Host Configuration Protocol 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DHCPCLI_H
#define _DHCPCLI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**** DEFINITIONS ****/

/* Opcodes */
#define DHCP_OP_REQUEST         1
#define DHCP_OP_REPLY           2

/* Hardware address type */
#define DHCP_HTYPE_ETH          1

/* Magic */
#define DHCP_MAGIC              0x63825363

/* Options */
#define DHCP_OPT_PADDING            0       // Padding
#define DHCP_OPT_SUBNET_MASK        1       // Subnet mask
#define DHCP_OPT_ROUTER             3       // Router
#define DHCP_OPT_DNS                6       // DNS
#define DHCP_OPT_REQUESTED_IP       50      // Requested IPv4 address
#define DHCP_OPT_LEASE_TIME         51      // Lease time
#define DHCP_OPT_MESSAGE_TYPE       53      // DHCP message type
#define DHCP_OPT_SERVER_ID          54      // Server ID
#define DHCP_OPT_PARAMETER_REQ      55      // Parameter request list
#define DHCP_OPT_END                255     // End of list

/* Message types */
#define DHCPDISCOVER                1
#define DHCPOFFER                   2
#define DHCPREQUEST                 3
#define DHCPDECLINE                 4
#define DHCPACK                     5
#define DHCPNAK                     6
#define DHCPRELEASE                 7

/**** TYPES ****/

typedef struct dhcp_packet {
    uint8_t opcode;             // Message opcode
    uint8_t htype;              // Hardware address type
    uint8_t hlen;               // Hardware address length
    uint8_t hops;               // Optionally used by relay agents
    uint32_t xid;               // Transaction ID
    uint16_t secs;              // Seconds elapsed since client began address acquisition or reneal
    uint16_t flags;             // Flags
    uint32_t ciaddr;            // Client IP addr
    uint32_t yiaddr;            // "Your" client IP addr
    uint32_t siaddr;            // Next server in bootstrap
    uint32_t giaddr;            // Gateway address
    uint8_t chaddr[6];          // Client MAC
    uint8_t reserved[10];       // Reserved
    char server_name[64];       // Server name
    char boot_file_name[128];   // Boot file name
    uint32_t magic;             // Magic cookie
    uint8_t options[256];       // Options
} __attribute__((packed)) dhcp_packet_t;

typedef struct dhcp_options {
    in_addr_t subnet_mask;      // Subnet mask
    in_addr_t server_addr;      // Server address
    in_addr_t *dns_start;       // DNS start
    in_addr_t *dns_end;         // DNS end
    in_addr_t *router_start;    // Router list
    in_addr_t *router_end;      // End of router list
    uint32_t lease_time;        // DHCP lease time
    uint8_t msg_type;           // Message type
} dhcp_options_t;

#endif