/**
 * @file drivers/usb/xhci/xhci_trb.h
 * @brief xHCI TRB
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _XHCI_TRB_H
#define _XHCI_TRB_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

/**
 * @brief Generic TRB. Contains nothing specific and just enough to schedule without knowing what they are
 */
typedef struct xhci_trb {
    uint64_t specific;          // TRB specific information
    uint32_t status;            // Status

    union {
        struct {
            uint32_t c:1;       // Cycle bit
            uint32_t rsvd:4;    // Reserved
            uint32_t ioc:1;     // Interrupt on completion
            uint32_t idt:1;     // Immediate data
            uint32_t rsvd1:3;   // Reserved
            uint32_t type:6;    // TRB type
            uint32_t rsvd2:16;  // Reserved
        } control;
        uint32_t control_raw;
    };
} __attribute__((packed)) xhci_trb_t;

/**
 * @brief Normal TRB
 * 
 * Used for bulk/interrupt transfers
 */
typedef struct xhci_normal_trb {
    uint64_t data_buffer;       // Data buffer pointer
    
    union {
        struct {
            uint32_t len:17;        // TRB transfer length
            uint32_t td_size:5;     // TD size
            uint32_t target:10;     // Interrupter target
        } status;
        uint32_t status_raw;
    };

    union {
        struct {
            uint32_t c:1;       // Cycle bit
            uint32_t ent:1;     // Evaluate next TRB
            uint32_t ns:1;      // No snoop
            uint32_t chain:1;   // Chain
            uint32_t ioc:1;     // Interrupt on completion
            uint32_t idt:1;     // Immediate data
            uint32_t rsvd1:2;   // Reserved
            uint32_t bei:1;     // Block event interrupt
            uint32_t type:6;    // TRB type
            uint32_t rsvd2:16;  // Reserved
        } control;

        uint32_t control_raw;
    };
} __attribute__((packed)) xhci_normal_trb_t;

/**
 * @brief Link TRB
 * 
 * Used to point to another TRB
 */
typedef struct xhci_link_trb {
    uint64_t ring_segment;          // Ring segment pointer
    
    union {
        struct {
            uint32_t reserved:22;   // Reserved
            uint32_t target:10;     // Interrupter target
        } status;

        uint32_t status_raw;
    };

    union {
        struct {
            uint32_t c:1;           // Cycle bit
            uint32_t tc:1;          // Toggle cycle
            uint32_t rsvd:2;        // Reserved
            uint32_t ch:1;          // Chain bit
            uint32_t ioc:1;         // Interrupt on comlpetion
            uint32_t rsvd1:4;       // Reserved
            uint32_t type:6;        // TRB type
            uint32_t rsvd2:16;      // Reserved
        } control;

        uint32_t control_raw;
    };
} __attribute__((packed)) xhci_link_trb_t;

typedef struct xhci_command_completion_request_block {
    uint64_t command_trb_pointer;
    struct {
        uint32_t rsvd0           : 24;
        uint32_t completion_code : 8;
    };
    struct {
        uint32_t cycle_bit   : 1;
        uint32_t rsvd1       : 9;
        uint32_t trb_type    : 6;
        uint32_t vfid        : 8;
        uint32_t slot_id     : 8;
    };
} xhci_command_completion_trb_t;





#endif