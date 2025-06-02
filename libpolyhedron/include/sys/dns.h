/**
 * @file libpolyhedron/include/sys/dns.h
 * @brief DNS protocol and information
 * 
 * THIS IS INTERNAL TO POLYHEDRON
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _SYS_DNS_H
#define _SYS_DNS_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/
#define DNS_FLAG_RD         (1 << 8) // All we need is RD (recursion desired)

/**** TYPES ****/

typedef struct dns_header {
    uint16_t xid;                   // Random transaction ID
    uint16_t flags;                 // Flags
    uint16_t questions;             // Amount of questions
    uint16_t answers;               // Amount of answers
    uint16_t authorities;           // Amount of authorities
    uint16_t additional;            // Additional
    uint8_t data[];                 // Variable-length data
} __attribute__((packed)) dns_header_t;

#endif