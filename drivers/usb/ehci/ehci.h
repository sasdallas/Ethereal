/**
 * @file drivers/usb/ehci/ehci.h
 * @brief Enhanced Host Controller Interface driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef EHCI_H
#define EHCI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/misc/pool.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

/* Host controller registers */
#define EHCI_REG_CAPLENGTH              0x00    // Capability register length
#define EHCI_REG_HCIVERSION             0x02    // Interface version number
#define EHCI_REG_HCSPARAMS              0x04    // Structural parameters
#define EHCI_REG_HCCPARAMS              0x08    // Capability parameters
#define EHCI_REG_HCSP_PORTROUTE         0x0C    // Companion port route description

/* Host controller operational registers */
#define EHCI_REG_USBCMD                 0x00    // USB command
#define EHCI_REG_USBSTS                 0x04    // USB status
#define EHCI_REG_USBINTR                0x08    // USB interrupt enable
#define EHCI_REG_FRINDEX                0x0C    // USB frame index
#define EHCI_REG_CTRLDSSEGMENT          0x10    // 4G segment selector
#define EHCI_REG_PERIODICLISTBASE       0x14    // Frame list base address
#define EHCI_REG_ASYNCLISTADDR          0x18    // Next asyncronous list address
#define EHCI_REG_CONFIGFLAG             0x40    // Configured flag register
#define EHCI_REG_PORTSC                 0x44    // Port status/control (base, each port should add 4 to this)

/* HCS parameters */
#define EHCI_HCSPARAMS_INDICATOR        0x10000     // Port indicator control
#define EHCI_HCSPARAMS_NCC              0x0F000     // Number of companion controllers
#define EHCI_HCSPARAMS_NPCC             0x00F00     // Number of ports per companion controller
#define EHCI_HCSPARAMS_ROUTE            0x00080     // Port routing rules
#define EHCI_HCSPARAMS_PPC              0x00010     // Port power control
#define EHCI_HCSPARAMS_NPORTS           0x0000F     // Number of ports

/* HCC parameters */
#define EHCI_HCCPARAMS_EECP             0x0F000     // EHCI extended capabilities pointer
#define EHCI_HCCPARAMS_IST              0x00F00     // Isochronous scheduling threshold
#define EHCI_HCCPARAMS_ASYNC            0x00004     // Asyncronous schedule park capability
#define EHCI_HCCPARAMS_PFL              0x00002     // Programmable frame list
#define EHCI_HCCPARAMS_64BIT            0x00001     // 64-bit addressing

/* USBCMD */
#define EHCI_USBCMD_ITC                 0xFF0000    // Interrupt threshold control (in micro frames)
#define EHCI_USBCMD_ASPME               0x000800    // Asyncronous schedule park mode enable
#define EHCI_USBCMD_ASPMC               0x000300    // Asyncronous schedule park mode count
#define EHCI_USBCMD_LHCR                0x000080    // Light host controller reset
#define EHCI_USBCMD_IOAAD               0x000040    // Interrupt on async advance doorbell
#define EHCI_USBCMD_ASE                 0x000020    // Asyncronous schedule enable
#define EHCI_USBCMD_PSE                 0x000010    // Periodic schedule enable
#define EHCI_USBCMD_FLS                 0x00000C    // Frame list size
#define EHCI_USBCMD_HCRESET             0x000002    // Host controller reset
#define EHCI_USBCMD_RS                  0x000001    // Run/stop

/* USBSTS */
#define EHCI_USBSTS_HCHALTED            0x010000    // Host controller halted
#define EHCI_USBSTS_IOAA                0x000020    // Interrupt on async advance
#define EHCI_USBSTS_HSE                 0x000010    // Host system error
#define EHCI_USBSTS_FLR                 0x000008    // Frame list rollover
#define EHCI_USBSTS_PCD                 0x000004    // Port change detect
#define EHCI_USBSTS_USBERRINT           0x000002    // USB error interrupt
#define EHCI_USBSTS_USBINT              0x000001    // USB interrut

/* USBINTR */
#define EHCI_USBINTR_IOAA               0x000020    // Interrupt on async advance
#define EHCI_USBINTR_HSE                0x000010    // Interrupt on host system error
#define EHCI_USBINTR_FLR                0x000008    // Interrupt on frame list rollover
#define EHCI_USBINTR_PCI                0x000004    // Interrupt on port change
#define EHCI_USBINTR_ERR                0x000002    // Interrupt on USB error
#define EHCI_USBINTR_USBINT             0x000001    // USB interrupt enable

/* PORTSC */
#define EHCI_PORTSC_WKOC_E              0x400000    // Wake on over-current enable
#define EHCI_PORTSC_WKDSCNNT_E          0x200000    // Wake on disconnect enable
#define EHCI_PORTSC_WKCNNT_E            0x100000    // Wake on connect enable
#define EHCI_PORTSC_TC                  0x0F0000    // Port test control
#define EHCI_PORTSC_IC                  0x00C000    // Port indicator control
#define EHCI_PORTSC_OWNER               0x002000    // Port owner
#define EHCI_PORTSC_PP                  0x001000    // Port power
#define EHCI_PORTSC_LS                  0x000C00    // Port line status
#define EHCI_PORTSC_RESET               0x000100    // Port reset
#define EHCI_PORTSC_SUSPEND             0x000080    // Port suspend
#define EHCI_PORTSC_FPR                 0x000040    // Force port resume
#define EHCI_PORTSC_OCC                 0x000020    // Over-current change
#define EHCI_PORTSC_OCA                 0x000010    // Over-current active
#define EHCI_PORTSC_ENABLE_CHANGE       0x000008    // Port enable/disable change
#define EHCI_PORTSC_ENABLE              0x000004    // Port enable/disable
#define EHCI_PORTSC_CONNECT_CHANGE      0x000002    // Port connect status change
#define EHCI_PORTSC_CONNECT             0x000001    // Current connection status

/* Bitshifts that weren't included */
#define EHCI_PORTSC_TC_SHIFT            16
#define EHCI_PORTSC_IC_SHIFT            14
#define EHCI_PORTSC_LS_SHIFT            10
#define EHCI_USBCMD_ITC_SHIFT           16
#define EHCI_USBCMD_ASPMC_SHIFT         8
#define EHCI_USBCMD_FLS_SHIFT           2
#define EHCI_HCCPARAMS_EECP_SHIFT       8
#define EHCI_HCSPARAMS_NCC_SHIFT        12
#define EHCI_HCSPARAMS_NPCC_SHIFT       8

/* Frame list element pointer */
#define EHCI_FLP_TYPE_ITD               0           // iTD (Isochronous)
#define EHCI_FLP_TYPE_QH                1           // QH (Queue Head)
#define EHCI_FLP_TYPE_SITD              2           // siTD (Split Transcation Isochronous TD)
#define EHCI_FLP_TYPE_FSTN              3           // FSTN (Frame Span Transversal Node)

/* Transfer types (internal) */
#define EHCI_TRANSFER_CONTROL           1           // Control
#define EHCI_TRANSFER_INTERRUPT         2           // Interrupt transfer

/* Packet types */
#define EHCI_PACKET_IN                  0x01
#define EHCI_PACKET_OUT                 0x00
#define EHCI_PACKET_SETUP               0x02

/* Legacy support */
#define USBLEGSUP                       0x00        // USBLEGSUP
#define USBLEGSUP_HC_BIOS               0x10000     // BIOS Owned Semaphore
#define USBLEGSUP_HC_OS                 0x1000000   // OS Owned Semaphore

/**** TYPES ****/

/**
 * @brief EHCI Queue Element Transfer Descriptor
 */
typedef struct ehci_td {
    union {
        struct {
            uint32_t terminate:1;       // Terminate (validity)
            uint32_t reserved:4;        // Reserved
            uint32_t lp:27;             // Link pointer - points to the next TD
        };

        volatile uint32_t raw;
    } link;

    union {
        struct {
            uint32_t terminate:1;       // Terminate (validity)
            uint32_t reserved:4;        // Reserved
            uint32_t lp:27;             // Link pointer - points to the next TD
        };

        volatile uint32_t raw;
    } alt_link;

    union {
        struct {
            uint32_t ping:1;            // Ping state
            uint32_t split:1;           // Split state
            uint32_t miss:1;            // Missed microframe
            uint32_t transaction:1;     // Transaction error
            uint32_t babble:1;          // Babble
            uint32_t data_buffer:1;     // Data buffer
            uint32_t halted:1;          // Halted
            uint32_t active:1;

            uint32_t pid:2;             // PID code
            uint32_t cerr:2;            // Error counter
            uint32_t page:3;            // Current page
            uint32_t ioc:1;             // Interrupt on complete

            uint32_t len:15;            // Total bytes to transfer
            uint32_t toggle:1;          // Data toggle
        };

        volatile uint32_t raw;
    } token;

    volatile uint32_t buffer[5];        // Buffer pages (NOTE: 0xFFF is reserved in everything except page #0)
    volatile uint32_t ext_buffer[5];    // (Appendix B, 64-bit) Extended buffer pages

    // Driver-defined
    uint32_t software_use[3];
} __attribute__((packed)) ehci_td_t;

/**
 * @brief EHCI Queue Head
 */
typedef struct ehci_qh {
    union {
        struct {
            uint32_t terminate:1;       // Terminate
            uint32_t select:2;          // Select
            uint32_t reserved:2;        // Reserved
            uint32_t qhlp:27;           // Queue head link pointer
        };

        volatile uint32_t raw;
    } qhlp;

    // Endpoint characteristics
    union {
        struct {
            uint32_t devaddr:7;         // Device address
            uint32_t i:1;               // Inactivate on next transcation
            uint32_t endpt:4;           // Endpoint number
            uint32_t eps:2;             // Endpoint speed
            uint32_t dtc:1;             // Data toggle control
            uint32_t h:1;               // Head of reclamation list
            uint32_t mps:11;            // Max packet size
            uint32_t c:1;               // Control endoint flag
            uint32_t rl:4;              // NAK count reload 
        };

        volatile uint32_t raw;
    } ch; 

    // Endpoint capabilities
    union {
        struct {
            uint32_t ism:8;             // Interrupt schedule mask
            uint32_t scm:8;             // Split completion mask
            uint32_t hub_addr:7;        // Hub address
            uint32_t port:7;            // Port number
            uint32_t mult:2;            // High-bandwidth pipe multiplier
        };

        volatile uint32_t raw;
    } cap;

    
    // Current td pointer
    union {
        struct {
            uint32_t terminate:1;       // Terminate
            uint32_t nakcnt:3;          // NAK count
            uint32_t lp:28;             // Link pointer
        };

        volatile uint32_t raw;
    } td_current;

    // Next td pointer
    union {
        struct {
            uint32_t terminate:1;       // Terminate
            uint32_t reserved:3;        // Reserved
            uint32_t lp:28;             // Link ointer
        };

        volatile uint32_t raw;
    } td_next;

    // Alternate next td pointer
    union {
        struct {
            uint32_t terminate:1;       // Terminate
            uint32_t nakcnt:3;          // NAK count
            uint32_t lp:28;             // Link pointer
        };

        volatile uint32_t raw;
    } td_next_alt;

    // Token
    union {
        struct {
            uint32_t ping:1;            // Ping state
            uint32_t split:1;           // Split state
            uint32_t miss:1;            // Missed microframe
            uint32_t transaction:1;     // Transaction error
            uint32_t babble:1;          // Babble
            uint32_t data_buffer:1;     // Data buffer
            uint32_t halted:1;          // Halted
            uint32_t active:1;          // Active

            uint32_t pid:2;             // PID code
            uint32_t cerr:2;            // Error counter
            uint32_t page:3;            // Current page
            uint32_t ioc:1;             // Interrupt on complete

            uint32_t len:15;            // Total bytes to transfer
            uint32_t toggle:1;          // Data toggle
        };

        volatile uint32_t raw;
    } token;

    volatile uint32_t buffer[5];        // Buffer pages (NOTE: 0xFFF is reserved in everything except page #0)
    volatile uint32_t ext_buffer[5];    // (Appendix B, 64-bit) Extended buffer pages

    // We have to align these on a boundary that is 32-byte aligned
    // NOTE: This will vary based on architectures
    USBTransfer_t   *transfer;
    list_t          *td_list;           // Transfer descriptor list for this QH (virtual addresses rather than the QE physical addresses)

#ifdef __ARCH_I386__
    uint32_t pad[13];
#elif defined(__ARCH_X86_64__)
    uint32_t pad[11];
#endif
} __attribute__((packed)) ehci_qh_t;

/**
 * @brief EHCI Frame List Pointer
 */
typedef union ehci_flp {
    struct {
        uint32_t terminate:1;           // Terminate
        uint32_t type:2;                // Type
        uint32_t reserved:2;            // Reserved
        uint32_t lp:27;                 // Link pointer
    };

    volatile uint32_t raw;
} ehci_flp_t;

/**
 * @brief EHCI controller structure
 */
typedef struct ehci {
    uintptr_t mmio_base;        // MMIO base
    uintptr_t op_base;          // Operational base
    ehci_flp_t *frame_list;     // Frame list

    // Asyncronous queue head
    ehci_qh_t *qh_async;        // Async QH

    // Pools
    pool_t *qh_pool;            // QH pool
    pool_t *td_pool;            // TD pool

    // Queue heads
    list_t *periodic_list;      // List of periodic queue heads
    list_t *async_list;         // List of asyncronous queue heads
} ehci_t;

/**** MACROS ****/

// Get the host controller
#define HC(con) ((ehci_t*)con->hc)

// Terminate
#define QH_LINK_TERM(qh) (qh->qhlp.terminate = 1)
#define TD_LINK_TERM(td) { td->link.terminate = 1; td->alt_link.terminate = 1; }

// Convert address to link pointer (bitshifting for the union)
#define LINK(addr) ((uint32_t)(arch_mmu_physical(NULL, (uintptr_t)addr) >> 5))
#define LINK2(addr) ((uint32_t)(arch_mmu_physical(NULL, (uintptr_t)addr) >> 4))

// Link together two queue heads
#define QH_LINK_QH(prev, next)     {prev->qhlp.select = EHCI_FLP_TYPE_QH; \
                                    prev->qhlp.terminate = 0; \
                                    prev->qhlp.qhlp = LINK(next); }

// Link together a queue head and a transfer descriptor (QE_LINK)
#define QH_LINK_TD(target_qh, td)  {target_qh->td_next.terminate = 0; \
                                    target_qh->td_next.lp = LINK2(td); \
                                    list_append(target_qh->td_list, td); }

// Link together a transfer descriptor and a transfer descriptor (using depth)
#define TD_LINK_TD(qh_link, prev, td)  {prev->link.terminate = 0; \
    prev->link.lp = LINK(td); \
    list_append(qh_link->td_list, td); }


/**** FUNCTIONS ****/

/**
 * @brief EHCI control transfer method
 */
int ehci_control(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer);

/**
 * @brief EHCI interrupt transfer method
 */
int ehci_interrupt(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer);

#endif