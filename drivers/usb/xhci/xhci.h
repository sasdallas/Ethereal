/**
 * @file drivers/usb/xhci/xhci.h
 * @brief eXtensible Host Controller Interface driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _XHCI_H
#define _XHCI_H

/**** INCLUDES ****/
#include <stdint.h>
#include "xhci_common.h"
#include "xhci_regs.h"
#include "xhci_util.h"
#include "xhci_ring.h"
#include "xhci_device.h"
#include <structs/list.h>
#include <kernel/misc/spinlock.h>
#include <kernel/misc/pool.h>

/**** TYPES ****/


typedef uintptr_t xhci_dcbaa_t;

typedef struct xhci {
    uint32_t pci_addr;              // PCI address in case any new calls need to be made
    uintptr_t mmio_addr;            // MMIO address
    
    // REGISTERS
    xhci_cap_regs_t *capregs;       // Capability registers
    xhci_op_regs_t *opregs;         // Operational registers
    xhci_runtime_regs_t *runtime;   // Runtime registers
    xhci_cap_regs_t capregs_save;   // Saved capability registers

    // POOLS
    pool_t *ctx_pool;               // Device context pool

    // COMMAND QUEUE SYSTEM
    list_t *event_queue;            // Other event queue
    list_t *command_queue;          // Command completion TRB queue
    int command_irq_handled;        // IRQ handled, new events available
    spinlock_t cmd_lock;            // Command lock

    // OTHER STRUCTURES
    xhci_dcbaa_t *dcbaa;            // DCBAA (physical)
    xhci_dcbaa_t *dcbaa_virt;       // DCBAA (virtual - this is an array of the virtual addresses stored in the DCBAA)
    xhci_cmd_ring_t *cmd_ring;      // Command ring
    xhci_event_ring_t *event_ring;  // Primary event ring
} xhci_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize an xHCI controller
 * @param device The PCI device of the xHCI controller
 */
int xhci_initController(uint32_t device);

/**
 * @brief Try to initialize an xHCI port
 * @param xhci The xHCI device to use
 * @param port The port number to attempt to initialize
 * @returns 0 on success
 */
int xhci_portInitialize(xhci_t *xhci, int port);

/**
 * @brief Send a command TRB to a controller
 * @param xhci The controller to send the command TRB to
 * @param trb The TRB to send to the controller
 * @returns 0 on success
 */
xhci_command_completion_trb_t *xhci_sendCommand(xhci_t *xhci, xhci_trb_t *trb);

#endif