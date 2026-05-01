/**
 * @file hexahedron/include/kernel/drivers/net/tcp.h
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

#ifndef DRIVERS_NET_TCP_H
#define DRIVERS_NET_TCP_H

/**** INCLUDES ****/

#include <stdint.h>
#include <kernel/drivers/net/ipv4.h>
#include <kernel/misc/util.h>

/**** DEFINITIONS ****/

#define TCP_FIN         0x01
#define TCP_SYN         0x02
#define TCP_RST         0x04
#define TCP_PSH         0x08
#define TCP_ACK         0x10
#define TCP_URG         0x20
#define TCP_ECE         0x40
#define TCP_CWR         0x80

#define TCP_OPT_END         0
#define TCP_OPT_NOP         1
#define TCP_OPT_MSS         2
#define TCP_OPT_WS          3
#define TCP_OPT_SACK_PERMIT 4
#define TCP_OPT_SACK        5
#define TCP_OPT_TIMESTAMPS  8
#define TCP_OPT_FASTOPEN    34

/**** TYPES ****/

typedef enum {
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSED
} tcp_state_t;

typedef struct tcp_packet {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t reserved:4;
    uint8_t data_offset:4;
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
    uint8_t options[0];
} tcp_packet_t;

typedef struct tcp_checksum_psuedo {
    uint32_t src;
    uint32_t dst;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t length;
} tcp_checksum_psuedo_t;

#endif
