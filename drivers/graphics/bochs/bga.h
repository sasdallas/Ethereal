/**
 * @file drivers/graphics/bochs/bga.h
 * @brief BGA header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_BOCHS_BGA_H
#define DRIVERS_BOCHS_BGA_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

// I/O ports
#define VBE_DISPI_IOPORT_INDEX      0x01CE
#define VBE_DISPI_IOPORT_DATA       0x01CF

// Indexes
#define VBE_DISPI_INDEX_ID              0
#define VBE_DISPI_INDEX_XRES            1
#define VBE_DISPI_INDEX_YRES            2
#define VBE_DISPI_INDEX_BPP             3
#define VBE_DISPI_INDEX_ENABLE          4
#define VBE_DISPI_INDEX_BANK            5
#define VBE_DISPI_INDEX_VIRT_WIDTH      6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     7
#define VBE_DISPI_INDEX_X_OFFSET        8
#define VBE_DISPI_INDEX_Y_OFFSET        9

// BPPs
#define VBE_DISPI_BPP_4                 0x04
#define VBE_DISPI_BPP_8                 0x08
#define VBE_DISPI_BPP_15                0x0F
#define VBE_DISPI_BPP_16                0x10
#define VBE_DISPI_BPP_24                0x18
#define VBE_DISPI_BPP_32                0x20

// Maxes
#define VBE_DISPI_MAX_XRES              1600
#define VBE_DISPI_MAX_YRES              1200

// Flags to VBE_DISPI_INDEX_ENABLE
#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_BANK_GRANULARITY_32K   0x10
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

// IDs
#define VBE_DISPI_ID0                    0xB0C0
#define VBE_DISPI_ID1                    0xB0C1
#define VBE_DISPI_ID2                    0xB0C2
#define VBE_DISPI_ID3                    0xB0C3
#define VBE_DISPI_ID4                    0xB0C4
#define VBE_DISPI_ID5                    0xB0C5

#endif
