/**
 * @file drivers/usb/xhci/xhci_definitions.h
 * @brief xHCI definitions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef XHCI_DEFINITIONS_H
#define XHCI_DEFINITIONS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/util.h>

/**** DEFINITIONS ****/

/* USBCMD */
#define XHCI_USBCMD_RS                  (1 << 0)
#define XHCI_USBCMD_HCRST               (1 << 1)
#define XHCI_USBCMD_INTE                (1 << 2)
#define XHCI_USBCMD_HSEE                (1 << 3)
#define XHCI_USBCMD_LHCRST              (1 << 7)
#define XHCI_USBCMD_CSS                 (1 << 8)
#define XHCI_USBCMD_CRS                 (1 << 9)
#define XHCI_USBCMD_EWE                 (1 << 10)
#define XHCI_USBCMD_EU3S                (1 << 11)
#define XHCI_USBCMD_CME                 (1 << 13)
#define XHCI_USBCMD_ETE                 (1 << 14)
#define XHCI_USBCMD_TSC_EN              (1 << 15)
#define XHCI_USBCMD_VTIOE               (1 << 16)

/* USBSTS */
#define XHCI_USBSTS_HCH                 (1 << 0)
#define XHCI_USBSTS_HSE                 (1 << 2)
#define XHCI_USBSTS_EINT                (1 << 3)
#define XHCI_USBSTS_PCD                 (1 << 4)
#define XHCI_USBSTS_SSS                 (1 << 8)
#define XHCI_USBSTS_RSS                 (1 << 9)
#define XHCI_USBSTS_SRE                 (1 << 10)
#define XHCI_USBSTS_CNR                 (1 << 11)
#define XHCI_USBSTS_HCE                 (1 << 12)

/* Extended capability ID */
#define XHCI_EXT_CAP_USBLEGSUP          1
#define XHCI_EXT_CAP_SUPPORTED          2

/* General */
#define XHCI_COMMAND_RING_TRB_COUNT     256
#define XHCI_EVENT_RING_TRB_COUNT       256
#define XHCI_TRANSFER_RING_TRB_COUNT    256

/* TRB types */
#define XHCI_TRB_TYPE_NORMAL            1
#define XHCI_TRB_TYPE_SETUP_STAGE       2
#define XHCI_TRB_TYPE_DATA_STAGE        3
#define XHCI_TRB_TYPE_STATUS_STAGE      4
#define XHCI_TRB_TYPE_ISOCH             5
#define XHCI_TRB_TYPE_LINK              6
#define XHCI_TRB_TYPE_EVENT_DATA        7
#define XHCI_TRB_TYPE_NO_OP             8

/* Commands */
#define XHCI_CMD_ENABLE_SLOT            9
#define XHCI_CMD_DISABLE_SLOT           10
#define XHCI_CMD_ADDRESS_DEVICE         11
#define XHCI_CMD_EVALUATE_CONTEXT       12
#define XHCI_CMD_RESET_ENDPOINT         13
#define XHCI_CMD_STOP_ENDPOINT          14

/* Events */
#define XHCI_EVENT_TRANSFER             32
#define XHCI_EVENT_COMMAND_COMPLETION   33
#define XHCI_EVENT_PORT_STATUS_CHANGE   34

/* IMAN */
#define XHCI_IMAN_INTERRUPT_PENDING     (1 << 0)
#define XHCI_IMAN_INTERRUPT_ENABLE      (1 << 1)

/* ERDP */
#define XHCI_ERDP_EHB                   (1 << 3)

/* PORTSC */
#define XHCI_PORTSC_CCS                 (1 << 0)
#define XHCI_PORTSC_PED                 (1 << 1)
#define XHCI_PORTSC_OCA                 (1 << 3)
#define XHCI_PORTSC_PR                  (1 << 4)
#define XHCI_PORTSC_PLS                 (1 << 5)    // 4-bit value
#define XHCI_PORTSC_PP                  (1 << 9)
#define XHCI_PORTSC_SPD                 (1 << 10)   // 4-bit value
#define XHCI_PORTSC_PIC                 (1 << 14)   // 2-bit value
#define XHCI_PORTSC_LWS                 (1 << 16)
#define XHCI_PORTSC_CSC                 (1 << 17)
#define XHCI_PORTSC_PEC                 (1 << 18)
#define XHCI_PORTSC_WRC                 (1 << 19)
#define XHCI_PORTSC_OCC                 (1 << 20)
#define XHCI_PORTSC_PRC                 (1 << 21)
// i dont care enough
#define XHCI_PORTSC_DR                  (1 << 30)
#define XHCI_PORTSC_WPR                 (1 << 31)

/* Speed */
#define XHCI_USB_SPEED_FULL_SPEED           1
#define XHCI_USB_SPEED_LOW_SPEED            2
#define XHCI_USB_SPEED_HIGH_SPEED           3
#define XHCI_USB_SPEED_SUPER_SPEED          4
#define XHCI_USB_SPEED_SUPER_SPEED_PLUS     5

/* Endpoint state */
#define XHCI_ENDPOINT_STATE_DISABLED    0
#define XHCI_ENDPOINT_STATE_RUNNING     1
#define XHCI_ENDPOINT_STATE_HALTED      2
#define XHCI_ENDPOINT_STATE_STOPPED     3
#define XHCI_ENDPOINT_STATE_ERROR       4

/* Endpoint type */
#define XHCI_ENDPOINT_TYPE_INVALID          0
#define XHCI_ENDPOINT_TYPE_ISOCH_OUT        1
#define XHCI_ENDPOINT_TYPE_BULK_OUT         2
#define XHCI_ENDPOINT_TYPE_INT_OUT          3
#define XHCI_ENDPOINT_TYPE_CONTROL          4
#define XHCI_ENDPOINT_TYPE_ISOCH_IN         5
#define XHCI_ENDPOINT_TYPE_BULK_IN          6
#define XHCI_ENDPOINT_TYPE_INT_IN           7


/**** TYPES ****/

typedef struct xhci_cap_regs {
    // NOTE: 32-bit reads are required here for QEMU (see https://bugs.launchpad.net/qemu/+bug/1693050)
    // NOTE: This driver is compiled with -fstrict-volatile-bitfields and the cap pointer is made volatile for this reason
    uint32_t caplength:8;
    uint32_t rsvd:8;
    uint32_t revision_minor:8;
    uint32_t revision_major:8;

    union {
        struct {
            uint32_t max_slots:8;
            uint32_t max_interrupters:11;
            uint32_t rsvd1:5;
            uint32_t max_ports:8;
        };

        uint32_t hcsparams1;
    };

    union {
        struct {
            uint32_t ist:4;
            uint32_t erst_max:4;
            uint32_t rsvd2:13;
            uint32_t max_scratchpad_buffers_hi:5;
            uint32_t scratchpad_restore:1;
            uint32_t max_scratchpad_buffers_lo:5;
        };

        uint32_t hcsparams2;
    };

    uint32_t hcsparams3;

    union {
        struct {
            uint32_t dontcare:2;
            uint32_t context_size:1;
            uint32_t dontcare2:13;
            uint32_t extended_cap_pointer:16;
        };

        uint32_t hccparams1;
    };
    
    
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} xhci_cap_regs_t;

STATIC_ASSERT(sizeof(xhci_cap_regs_t) == 32);

typedef struct xhci_port_regs {
    uint32_t portsc;                // Port status and control
    uint32_t portpmsc;              // Port power management status and control
    uint32_t portli;                // Port link info
    uint32_t porthlpmc;             // Port hardware LPM control
} xhci_port_regs_t;

STATIC_ASSERT(sizeof(xhci_port_regs_t) == 16);

typedef struct xhci_op_regs {
    uint32_t usbcmd;                // USB command
    uint32_t usbsts;                // USB status
    uint32_t pagesize;              // Page size
    uint32_t rsvd0;                 // Reserved
    uint32_t rsvd1;                 
    uint32_t dnctrl;                // Device notification control
    uint32_t crcr_lo;
    uint32_t crcr_hi;
    uint32_t rsvd2;                 // Reserved
    uint32_t rsvd3;
    uint32_t rsvd4;
    uint32_t rsvd5; 
    uint64_t dcbaap;                // Device context base address array pointer
    uint32_t config;                // Configure
    uint32_t reserved[241];         // Reserved
    xhci_port_regs_t ports[];       // Ports
} xhci_op_regs_t;

STATIC_ASSERT(sizeof(xhci_op_regs_t) == 1024);

typedef struct xhci_int_regs {
    uint32_t iman;                  // Interrupter management
    uint32_t imod;                  // Interrupter moderation
    uint32_t erstsz;                // Event ring segment table size
    uint32_t rsvd0;                 // Reserved
    uint64_t erstba;                // Event ring segment table base address
    uint64_t erdp;                  // Event ring dequeue pointer
} xhci_int_regs_t;

STATIC_ASSERT(sizeof(xhci_int_regs_t) == 0x20);

typedef struct xhci_runtime_regs {
    uint32_t mfindex;
    uint32_t rsvd[7];
    xhci_int_regs_t irs[];
} xhci_runtime_regs_t;

STATIC_ASSERT(sizeof(xhci_runtime_regs_t) == 0x20);

typedef struct xhci_trb {
    uint64_t parameter;
    uint32_t status;

    union {
        struct {
            uint32_t c:1;
            uint32_t ent:1;
            uint32_t rsvd:8;
            uint32_t type:6;
            uint32_t rsvd2:16;
        };

        uint32_t control;
    };
} xhci_trb_t;

STATIC_ASSERT(sizeof(xhci_trb_t) == 0x10);

typedef struct xhci_command_completion_trb {
    uint64_t ctrb;

    union {
        struct {
            uint32_t rsvd0:24;
            uint32_t cc:8;
        };

        uint32_t status;
    };
    
    union {
        struct {
            uint32_t c:1;
            uint32_t reserved2:9;
            uint32_t type:6;
            uint32_t vfid:8;
            uint32_t slot_id:8;
        };

        uint32_t control;
    };
} xhci_command_completion_trb_t;

STATIC_ASSERT(sizeof(xhci_command_completion_trb_t) == 0x10);

typedef struct xhci_link_trb {
    uint64_t ring_segment;

    uint32_t rsvd0:22;              
    uint32_t interrupter_target:10;

    union {
        struct {
            uint32_t c:1;
            uint32_t tc:1;
            uint32_t rsvd1:2;
            uint32_t ch:1;
            uint32_t ioc:1;
            uint32_t rsvd2:4;
            uint32_t type:6;
            uint32_t rsvd3:16;
        };

        uint32_t control;
    };
} xhci_link_trb_t;

STATIC_ASSERT(sizeof(xhci_link_trb_t) == 0x10);

typedef struct xhci_port_status_change_trb {
    uint32_t rsvd0:24;
    uint32_t port_id:8;

    uint32_t rsvd1:32;

    uint32_t rsvd2:24;
    uint32_t cc:8;

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd3:9;
            uint32_t type:6;
            uint32_t rsvd4:16;
        };

        uint32_t control;
    };
} xhci_port_status_change_trb_t;

STATIC_ASSERT(sizeof(xhci_port_status_change_trb_t) == 0x10);

typedef struct xhci_enable_slot_trb {
    uint32_t rsvd0;
    uint32_t rsvd1;
    uint32_t rsvd2;

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd3:9;
            uint32_t type:6;
            uint32_t slot_type:5;
            uint32_t rsvd4:11;
        };

        uint32_t control;
    };
} xhci_enable_slot_trb_t;  

STATIC_ASSERT(sizeof(xhci_enable_slot_trb_t) == 0x10);

typedef struct xhci_address_device_trb {
    uint64_t input_ctx;                 // Physical base of input context
    uint32_t rsvd0;                     // Reserved

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd1:8;
            uint32_t bsr:1;
            uint32_t type:6;
            uint32_t rsvd2:8;
            uint32_t slot_id:8;
        };

        uint32_t control;
    };
} xhci_address_device_trb_t;

STATIC_ASSERT(sizeof(xhci_address_device_trb_t) == 0x10);

typedef struct xhci_setup_stage_trb {
    uint32_t bmRequestType:8;
    uint32_t bRequest:8;
    uint32_t wValue:16;
    
    uint32_t wIndex:16;
    uint32_t wLength:16;

    uint32_t transfer_length:17;
    uint32_t rsvd0:5;
    uint32_t interrupter:10;

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd1:4;
            uint32_t ioc:1;
            uint32_t idt:1;
            uint32_t rsvd2:3;
            uint32_t type:6;
            uint32_t trt:2;
            uint32_t rsvd3:14;
        };

        uint32_t control;
    };
} xhci_setup_stage_trb_t;

STATIC_ASSERT(sizeof(xhci_setup_stage_trb_t) == 0x10);

typedef struct xhci_transfer_completion_trb {
    uint64_t buffer;

    struct {
        uint32_t transfer_len:24;
        uint32_t completion_code:8;
    };

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd0:1;
            uint32_t event_data:1;
            uint32_t rsvd1:7;
            uint32_t type:6;
            uint32_t endpoint_id:5;
            uint32_t rsvd2:3;
            uint32_t slot_id:8;
        };
    };
} xhci_transfer_completion_trb_t;

STATIC_ASSERT(sizeof(xhci_transfer_completion_trb_t) == 0x10);

typedef struct xhci_data_stage_trb {
    uint64_t buffer;

    uint32_t transfer_length:17;
    uint32_t td_size:5;
    uint32_t interrupter:10;

    union {
        struct {
            uint32_t c:1;
            uint32_t ent:1;
            uint32_t isp:1;
            uint32_t ns:1;
            uint32_t ch:1;
            uint32_t ioc:1;
            uint32_t idt:1;
            uint32_t rsvd0:3;
            uint32_t type:6;
            uint32_t dir:2;
            uint32_t rsvd1:14;
        };

        uint32_t control;
    };
} xhci_data_stage_trb_t;

STATIC_ASSERT(sizeof(xhci_data_stage_trb_t) == 0x10);

typedef struct xhci_status_stage_trb {
    uint64_t rsvd0;

    uint32_t rsvd1:22;
    uint32_t interrupter:10;
    
    union {
        struct {
            uint32_t c:1;
            uint32_t ent:1;
            uint32_t rsvd2:2;
            uint32_t ch:1;
            uint32_t ioc:1;
            uint32_t rsvd3:4;
            uint32_t type:6;
            uint32_t dir:1;
            uint32_t rsvd4:15;
        };

        uint32_t control;
    };
} xhci_status_stage_trb_t;

STATIC_ASSERT(sizeof(xhci_status_stage_trb_t) == 0x10);

typedef struct xhci_evaluate_context_trb {
    uint64_t input_context;
    uint32_t rsvd0;

    union {
        struct {
            uint32_t c:1;
            uint32_t rsvd1:8;
            uint32_t bsr:1;
            uint32_t type:6;
            uint32_t rsvd2:8;
            uint32_t slot_id:8;
        };

        uint32_t control;
    };
} xhci_evaluate_context_trb_t;

STATIC_ASSERT(sizeof(xhci_evaluate_context_trb_t) == 0x10);


typedef struct xhci_event_ring_entry {
    uint64_t rsba;
    uint32_t rsz;
    uint32_t rsvd0;
} xhci_event_ring_entry_t;

STATIC_ASSERT(sizeof(xhci_event_ring_entry_t) == 0x10);

typedef struct xhci_extended_capability {
    uint32_t id:8;
    uint32_t next:8;
    uint32_t reserved:16;
} xhci_extended_capability_t;

STATIC_ASSERT(sizeof(xhci_extended_capability_t) == sizeof(uint32_t));

typedef struct xhci_legsup_capability {
    uint32_t id:8;
    uint32_t next:8;
    uint32_t bios_sem:1;
    uint32_t rsvd0:7;
    uint32_t os_sem:1;
    uint32_t rsvd1:7;
} xhci_legsup_capability_t;

STATIC_ASSERT(sizeof(xhci_legsup_capability_t) == sizeof(uint32_t));

typedef struct xhci_supported_prot_capability {
    uint32_t id:8;
    uint32_t next:8;
    uint32_t minor:8;
    uint32_t major:8;

    uint32_t name_string:32;

    uint32_t compat_port_offset:8;
    uint32_t compat_port_count:8;
    uint32_t specific:8;
    uint32_t psic:8;
} xhci_supported_prot_capability_t;

STATIC_ASSERT(sizeof(xhci_supported_prot_capability_t) == sizeof(uint32_t)*3);

typedef struct xhci_input_context {
    uint32_t drop_flags;
    uint32_t add_flags;

    uint32_t rsvd0;
    uint32_t rsvd1;
    uint32_t rsvd2;
    uint32_t rsvd3;
    uint32_t rsvd4;

    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t rsvd5;
} xhci_input_context_t;

STATIC_ASSERT(sizeof(xhci_input_context_t) == 0x20);

typedef struct xhci_endpoint_context {
    uint32_t state:3;
    uint32_t rsvd0:5;
    uint32_t mult:2;
    uint32_t max_primary_streams:5;
    uint32_t linear_stream_array:1;
    uint32_t interval:8;
    uint32_t max_esit_payload_hi:8;

    uint32_t rsvd1:1;
    uint32_t error_count:2;
    uint32_t endpoint_type:3;
    uint32_t rsvd2:1;
    uint32_t host_initiate_disable:1;
    uint32_t max_burst_size:8;
    uint32_t max_packet_size:16;

    uint64_t transfer_ring_dequeue_ptr;     
    
    uint32_t average_trb_length:16;
    uint32_t max_esit_payload_lo:16;

    uint32_t rsvd3;
    uint32_t rsvd4;
    uint32_t rsvd5;
} xhci_endpoint_context_t;

STATIC_ASSERT(sizeof(xhci_endpoint_context_t) == 0x20);

typedef struct xhci_slot_context {
    uint32_t route_string   : 20;
    uint32_t speed          : 4;
    uint32_t rz             : 1;
    uint32_t mtt            : 1;
    uint32_t hub            : 1;
    uint32_t context_entries : 5;


    uint16_t    max_exit_latency;
    uint8_t     root_hub_port_num;
    uint8_t     port_count;

    uint32_t parent_hub_slot_id  : 8;
    uint32_t parent_port_number  : 8;
    uint32_t tt_think_time       : 2;
    uint32_t rsvd0               : 4;
    uint32_t interrupter_target  : 10;

    uint32_t device_address  : 8;
    uint32_t rsvd1           : 19;
    uint32_t slot_state      : 5;
    
    uint32_t rsvd2;
    uint32_t rsvd3;
    uint32_t rsvd4;
    uint32_t rsvd5;
} xhci_slot_context_t;

STATIC_ASSERT(sizeof(xhci_slot_context_t) == 0x20);

/**** MACROS ****/
#define TRB_SUCCESS(trb) ((trb)->cc == 1)

#endif