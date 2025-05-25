/**
 * @file drivers/net/rtl8169/rtl8169.h
 * @brief RTL8169 network card driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _RTL8169_H
#define _RTL8169_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/nic.h>

/**** DEFINITIONS ****/




/**** TYPES ****/

typedef struct rtl8169_desc {
    uint32_t flags;             // Flags
    uint32_t vlan;              // VLAN
    uint32_t buffer_lo;         // Low bits of buffer
    uint32_t buffer_hi;         // High bits of buffer
} __attribute__((packed)) rtl8169_desc_t;

#endif