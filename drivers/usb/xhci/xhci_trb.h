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

/**
 * @brief Command completion TRB
 * 
 * Signifies a command has been completed
 */
typedef struct xhci_command_completion_trb {
    uint64_t command_trb;
    struct {
        uint32_t reserved:24;
        uint32_t completion_code:8;
    };
    struct {
        uint32_t c:1;
        uint32_t reserved2:9;
        uint32_t type:6;
        uint32_t vfid:8;
        uint32_t slot_id:8;
    };
} __attribute__((packed)) xhci_command_completion_trb_t;

/**
 * @brief Address device TRB
 * 
 * Used to address a specific USB device
 */
typedef struct xhci_address_device_trb {
    uint64_t input_ctx;             // Physical base of input context
    uint32_t reserved;              // Reserved

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved2:8;       // Reserved
        uint32_t bsr:1;             // Block SET_ADDRESS
        uint32_t type:6;            // TRB type
        uint32_t reserved3:8;       // Reserved
        uint32_t slot_id:8;         // Slot ID
    };
} __attribute__((packed)) xhci_address_device_trb_t;

/**
 * @brief SETUP stage TRB
 */
typedef struct xhci_setup_trb {
    // These are just straight out of the USB request
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;

    // Other actual xHCI specific things
    struct {
        uint32_t transfer_len:17;   // TRB transfer length - always 8
        uint32_t reserved:5;        // Reserved
        uint32_t interrupter:10;    // Interrupter target
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved2:4;       // Reserved
        uint32_t ioc:1;             // Interrupt on complete
        uint32_t idt:1;             // Immediate data
        uint32_t reserved3:3;       // Reserved
        uint32_t type:6;            // TRB type
        uint32_t trt:2;             // Transfer type
        uint32_t reserved4:14;      // Reserved
    };
} __attribute__((packed)) xhci_setup_trb_t;


/**
 * @brief DATA stage TRB
 */
typedef struct xhci_data_trb {
    uint64_t buffer;                // Physical buffer address

    struct {
        uint32_t transfer_len:17;   // TRB transfer length - always 8
        uint32_t td_size:5;         // TD size, i.e. the number of packets remaining in the TD
        uint32_t interrupter:10;    // Interrupter target
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t ent:1;             // Evaluate next TRB
        uint32_t isp:1;             // Interrupt on short packet
        uint32_t ns:1;              // No snoop
        uint32_t ch:1;              // Chain
        uint32_t ioc:1;             // Interrupt on complete
        uint32_t idt:1;             // Immediate data
        uint32_t reserved3:3;       // Reserved
        uint32_t type:6;            // TRB type
        uint32_t dir:2;             // Transfer direction
        uint32_t reserved4:14;      // Reserved
    };
} __attribute__((packed)) xhci_data_trb_t;

/**
 * @brief STATUS stage TRB
 */
typedef struct xhci_status_trb {
    uint64_t reserved;              // Reserved
    
    struct {
        uint32_t reserved2:22;      // Reserved
        uint32_t interrupter:10;    // Interrupter target
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t ent:1;             // Evaluate next TRB
        uint32_t reserved3:2;       // Reserved
        uint32_t ch:1;              // Chain
        uint32_t ioc:1;             // Interrupt on complete
        uint32_t reserved4:4;       // Reserved
        uint32_t type:6;            // TRB type
        uint32_t dir:1;             // Transfer direction
        uint32_t reserved5:15;      // Reserved
    };
} __attribute__((packed)) xhci_status_trb_t;

/**
 * @brief Transfer completion TRB
 */
typedef struct xhci_transfer_completion_trb {
    uint64_t buffer;                // Transfer TRB pointer

    struct {
        uint32_t transfer_len:24;   // TRB transfer length - always 8
        uint32_t completion_code:8; // Completion code
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved:1;        // Reserved
        uint32_t event_data:1;      // Event data
        uint32_t reserved2:7;       // Reserved
        uint32_t type:6;            // TRB type
        uint32_t endpoint_id:5;     // Transfer 
        uint32_t reserved3:3;       // Reserved
        uint32_t slot_id:8;         // Slot ID
    };
} __attribute__((packed)) xhci_transfer_completion_trb_t;

/**
 * @brief Port status change TRB
 */
typedef struct xhci_port_status_change_trb {
    struct {
        uint32_t reserved:24;       // Reserved
        uint32_t port_id:8;         // Port ID
    };

    uint32_t reserved2;             // Reserved

    struct {
        uint32_t reserved3:24;      // Reseved
        uint32_t completion_code:8; // Completion code
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved4:9;       // Reserved
        uint32_t type:6;            // Type
        uint32_t reserved5:16;      // Reserved
    };
} __attribute__((packed)) xhci_port_status_change_trb_t;


/**
 * @brief Event data TRB
 */
typedef struct xhci_event_data_trb {
    uint64_t data;

    struct {
        uint32_t reserved:22;
        uint32_t interrupter:10;
    };

    struct {
        uint32_t c:1;               // Cycle
        uint32_t ent:1;             // Evaluate next TRB
        uint32_t reserved2:2;       // Reserved
        uint32_t ch:1;              // Chain
        uint32_t ioc:1;             // Interrupt on complete
        uint32_t reserved3:3;       // Reserved
        uint32_t bei:1;             // Block Event Interrupt
        uint32_t type:6;            // TRB type
        uint32_t reserved4:16;      // Reserved
    };
} __attribute__((packed)) xhci_event_data_trb_t;

/**
 * @brief Evaluate Context command TRB
 */
typedef struct xhci_evaluate_context_trb {
    uint64_t input_context;         // Inupt context
    uint32_t reserved;              // Reserved

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved2:8;       // Reserved
        uint32_t bsr:1;             // BSR (unused)
        uint32_t type:6;            // TRB type
        uint32_t reserved3:8;       // Reserved
        uint32_t slot_id:8;         // Slot ID
    };
} __attribute__((packed)) xhci_evaluate_context_trb_t;

/**
 * @brief Configure Endpoint command
 */
typedef struct xhci_configure_endpoint_trb {
    uint64_t input_context;         // Input context
    uint32_t reserved;              // Reserved

    struct {
        uint32_t c:1;               // Cycle
        uint32_t reserved2:8;       // Reserved
        uint32_t d:1;               // Deconfigure
        uint32_t type:6;            // Type
        uint32_t reserved3:8;       // Reserved
        uint32_t slot_id:8;         // Slot ID
    };
} __attribute__((packed)) xhci_configure_endpoint_trb_t;

/**
 * @brief Normal TRB
 * 
 * Used for bulk/interrupt transfers
 */
typedef struct xhci_normal_trb {
    uint64_t data_buffer;       // Data buffer pointer
    
    struct {
        uint32_t len:17;            // TRB transfer length
        uint32_t td_size:5;         // TD size
        uint32_t target:10;         // Interrupter target
    };

    struct {
        uint32_t c:1;               // Cycle bit
        uint32_t ent:1;             // Evaluate next TRB
        uint32_t isp:1;             // Interrupt on short packet
        uint32_t ns:1;              // No snoop
        uint32_t chain:1;           // Chain
        uint32_t ioc:1;             // Interrupt on completion
        uint32_t idt:1;             // Immediate data
        uint32_t reserved:2;        // Reserved
        uint32_t bei:1;             // Block event interrupt
        uint32_t type:6;            // TRB type
        uint32_t dir:1;             // Direction
        uint32_t reserved2:15;      // Reserved
    };
} __attribute__((packed)) xhci_normal_trb_t;

#endif