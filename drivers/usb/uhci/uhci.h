/**
 * @file drivers/uhci/uhci.h
 * @brief UHCI header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef UHCI_H
#define UHCI_H

/**** INCLUDES ****/

#include <stdint.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/dev.h>
#include <kernel/misc/pool.h>
#include <structs/list.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "Please define port I/O functions for UHCI driver"
#endif

/**** DEFINITIONS ****/

/* UHCI registers */

#define UHCI_REG_USBCMD         0x00    // Command register
#define UHCI_REG_USBSTS         0x02    // Status register
#define UHCI_REG_USBINTR        0x04    // Interrupt enable
#define UHCI_REG_FRNUM          0x06    // Frame number
#define UHCI_REG_FLBASEADD      0x08    // Frame list base address
#define UHCI_REG_SOFMOD         0x0C    // Start of frame modify
#define UHCI_REG_PORTSC1        0x10    // Port 1 status/control
#define UHCI_REG_PORTSC2        0x12    // Port 2 status/control
#define UHCI_REG_LEGSUP         0xC0    // Legacy support

/* USBCMD bitflags */

#define UHCI_CMD_RS                     (1 << 0)    // Run/stop
#define UHCI_CMD_HCRESET                (1 << 1)    // Host controller reset
#define UHCI_CMD_GRESET                 (1 << 2)    // Global reset
#define UHCI_CMD_EGSM                   (1 << 3)    // Enter global suspend resume
#define UHCI_CMD_FGR                    (1 << 4)    // Force global resume
#define UHCI_CMD_SWDBG                  (1 << 5)    // Software Debug
#define UHCI_CMD_CF                     (1 << 6)    // Configure
#define UHCI_CMD_MAXP                   (1 << 7)    // Max packet, 0 = 32, 1 = 64

/* USBSTS bitflags */

#define UHCI_STS_USBINT                 (1 << 0)    // USB interrupt
#define UHCI_STS_ERROR                  (1 << 1)    // USB error interrupt
#define UHCI_STS_RD                     (1 << 2)    // Resume detect
#define UHCI_STS_HSE                    (1 << 3)    // Host system error
#define UHCI_STS_HCPE                   (1 << 4)    // Host controller process error
#define UHCI_STS_HCH                    (1 << 5)    // HC halted

/* USBINTR bitflags */

#define UHCI_INTR_TIMEOUT               (1 << 0)    // Timeout/CRC interrupt enable
#define UHCI_INTR_RESUME                (1 << 1)    // Resume interrupt enable
#define UHCI_INTR_IOC                   (1 << 2)    // Interrupt on Complete Enable
#define UHCI_INTR_SP                    (1 << 3)    // Short packet interrupt enable

/* PORTSC bitflags */

#define UHCI_PORT_CONNECTION            (1 << 0)    // Current connect status
#define UHCI_PORT_CONNECTION_CHANGE     (1 << 1)    // Connect status change
#define UHCI_PORT_ENABLE                (1 << 2)    // Port enabled
#define UHCI_PORT_ENABLE_CHANGE         (1 << 3)    // Port status change
#define UHCI_PORT_LS                    (1 << 4)    // Line status
#define UHCI_PORT_RD                    (1 << 6)    // Resume detect
#define UHCI_PORT_LSDA                  (1 << 8)    // Low speed device attached
#define UHCI_PORT_RESET                 (1 << 9)    // Port reset
#define UHCI_PORT_SUSP                  (1 << 12)   // Port suspend
#define UHCI_PORT_RWC                   (UHCI_PORT_CONNECTION_CHANGE | UHCI_PORT_ENABLE_CHANGE)

/* UHCI packet ID */
#define UHCI_PACKET_IN                  0x69
#define UHCI_PACKET_OUT                 0xE1
#define UHCI_PACKET_SETUP               0x2D

/**** TYPES ****/

/**
 * @brief Frame list pointer
 */
typedef union uhci_flp {
    struct {
        uint32_t terminate:1;  // Terminate - if set, empty frame (invalid ptr), else pointer is valid
        uint32_t qh:1;         // QH/TD select (1 = QH)
        uint32_t reserved:2;   // Reserved
        uint32_t flp:28;       // Frame list pointer
    };

    uint32_t flp_raw;
} uhci_flp_t;

/**
 * @brief Transfer descriptor
 * 
 * @param link TD link pointer
 * @param cs TD control and status
 * @param token TD token
 * @param buffer TD buffer
 * 
 * Express the characteristics of the transaction requested on USB by a client.
 * Always aligned on a 16-byte boundary
 */
typedef struct uhci_td {
    union {
        struct {
            uint32_t terminate:1;      // Terminate (validity)
            uint32_t qh:1;             // QH/TD select
            uint32_t vf:1;             // Depth/breadth select
            uint32_t reserved:1;       // Reserved
            uint32_t lp:28;            // Link pointer - points to another TD/QH
        };

        uint32_t raw;
    } link;

    union {
        struct {
            uint32_t actlen:11;        // Actual length
            uint32_t reserved:6;       // Reserved (+1 for the reserved bit in status)
            
            // The following fields are all part of "status"
            uint32_t bitstuff:1;       // Bitstuff Error (Receive data stream contained a sequence of >6 ones in a row)
            uint32_t crc:1;            // CRC/Timeout Error
            uint32_t nak:1;            // NAK received
            uint32_t babble:1;         // Babble
            uint32_t data_buffer:1;    // Data Buffer Error
            uint32_t stalled:1;        // Stalled (serious error)
            uint32_t active:1;         // Active

            uint32_t ioc:1;            // Interrupt on Complete
            uint32_t ios:1;            // Isochronous select
            uint32_t ls:1;             // Low speed device
            uint32_t errors:2;         // Error count
            uint32_t spd:1;            // Short packet detect
            uint32_t reserved2:2;      // Reserved
        };

        uint32_t raw;
    } cs;

    union {
        struct {
            uint32_t pid:8;            // Packet ID
            uint32_t device_addr:7;    // Device address
            uint32_t endpt:4;          // Endpoint
            uint32_t d:1;              // Data toggle
            uint32_t reserved:1;       // Reserved
            uint32_t maxlen:11;        // Maximum length      
        };

        uint32_t raw;
    } token;

    uint32_t buffer;                    // Buffer pointer
    
    // Driver-defined
    uint32_t software_use[4];           // 4 DWORDs for software use
} uhci_td_t;

/**
 * @brief Queue head
 * 
 * @param qh_link Queue head link pointer
 * @param qe_link Queue element link pointer 
 * 
 * Queue heads are special structures uses to support the requirements of certain transers.
 * Always aligned on a 16-byte boundary
 */
typedef struct uhci_qh {
    union {
        struct {
            uint32_t terminate:1;      // Terminate (validity)
            uint32_t qh:1;             // QH/TD select
            uint32_t reserved:2;       // Reserved
            uint32_t qhlp:28;          // Queue head link pointer
        };

        uint32_t raw;
    } qh_link;

    union {
        struct {
            uint32_t terminate:1;      // Terminate (validity)
            uint32_t qh:1;             // QH/TD select
            uint32_t reserved:2;       // Reserved
            uint32_t qelp:28;          // Queue element link pointer
        };

        uint32_t raw;
    } qe_link;    

    // We have to align these on a 16/32-byte boundary, and we're only using 8 bytes right now, sooo..
    // NOTE: This will vary based on architectures
    USBTransfer_t   *transfer;          // Pointer to the current transfer
    list_t          *td_list;           // Transfer descriptor list for this QH (virtual addresses rather than the QE physical addresses)

#if defined(__ARCH_X86_64__)
    uint64_t        extra_qword;        // On 64-bit architectures, the transfer/td_list pointers will be 16 bytes and qh/qe link will only be 8, making total 24 and not 16 aligned. 
#endif
} uhci_qh_t;

/**
 * @brief UHCI controller
 */
typedef struct uhci {
    uint32_t io_addr;                   // I/O base address
    uhci_flp_t *frame_list;             // Frame list (should be 4KB aligned)
    spinlock_t lock;

    // Pools
    pool_t *qh_pool;                    // 16-byte aligned queue head pool (DMA)
    pool_t *td_pool;                    // 16-byte aligned transfer descriptor pool (DMA)

    // Queue heads
    list_t *qh_list;                    // List of queue heads
} uhci_t;

/**** MACROS ****/

// Get the host controller
#define HC(con) ((uhci_t*)con->hc)

// Set the terminate bit in a queue head/transfer descriptor
#define QH_LINK_TERM(qh) (qh->qh_link.terminate = 1)
#define TD_LINK_TERM(td) (td->link.terminate = 1)

// Convert address to link pointer (bitshifting for the union)
#define LINK(addr) ((uint32_t)(mem_getPhysicalAddress(NULL, (uintptr_t)addr) >> 4))

// Link together a queue head and a transfer descriptor (QE_LINK)
#define QH_LINK_TD(target_qh, td)  {target_qh->qe_link.qh = 0; \
                                    target_qh->qe_link.terminate = 0; \
                                    target_qh->qe_link.qelp = LINK(td); \
                                    list_append(target_qh->td_list, td); }

// Link together two queue heads
#define QH_LINK_QH(prev, next)     {prev->qh_link.qh = 1; \
                                    prev->qh_link.terminate = 0; \
                                    prev->qh_link.qhlp = LINK(next); }


// Link together a transfer descriptor and a transfer descriptor (using depth)
#define TD_LINK_TD(qh_link, prev, td)  {prev->link.qh = 0; \
                                        prev->link.terminate = 0; \
                                        prev->link.vf = 1; \
                                        prev->link.lp = LINK(td); \
                                        list_append(qh_link->td_list, td); }

/**** FUNCTIONS ****/



/**
 * @brief UHCI control transfer method
 */
int uhci_control(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer);

/**
 * @brief UHCI interrupt transfer method
 */
int uhci_interrupt(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer);

#endif