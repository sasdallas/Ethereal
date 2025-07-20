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
#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/pci.h>
#include <kernel/task/process.h>

/**** TYPES ****/


typedef uintptr_t xhci_dcbaa_t;

typedef struct xhci {
    pci_device_t *pci;              // PCI address in case any new calls need to be made
    uintptr_t mmio_addr;            // MMIO address
    USBController_t *controller;    // Controller object

    // REGISTERS
    xhci_cap_regs_t *capregs;       // Capability registers
    xhci_op_regs_t *opregs;         // Operational registers
    xhci_runtime_regs_t *runtime;   // Runtime registers
    xhci_cap_regs_t capregs_save;   // Saved capability registers

    // POOLS
    pool_t *ctx_pool;               // Device context pool
    pool_t *input_ctx_pool;         // Input context pool

    // COMMAND QUEUE SYSTEM
    list_t *event_queue;            // Other event queue
    list_t *transfer_queue;         // Transfer success TRB queue
    list_t *command_queue;          // Command completion TRB queue
    list_t *port_queue;             // Port event queue
    int available;                  // Events handled, new available
    spinlock_t cmd_lock;            // Command lock

    // OTHER STRUCTURES
    xhci_dcbaa_t *dcbaa;            // DCBAA (physical)
    xhci_dcbaa_t *dcbaa_virt;       // DCBAA (virtual - this is an array of the virtual addresses stored in the DCBAA)
    xhci_cmd_ring_t *cmd_ring;      // Command ring
    xhci_event_ring_t *event_ring;  // Primary event ring
    uint8_t bit64;                  // Controller is 64-bit and we need to use 64-bit style structures. This is ugly.
} xhci_t;

struct xhci_endpoint;

typedef struct xhci_dev {
    xhci_t *xhci;                           // xHCI controller parent
    int port;                               // Port number
    uint8_t slot_id;                        // xHCI slot ID
    xhci_input_context32_t *input_ctx;      // Input context
    uintptr_t input_ctx_phys;               // Physical address of input context
    xhci_transfer_ring_t *control_ring;     // Control endpoint transfer ring
    struct xhci_endpoint *endp[32];         // Endpoints to use
} xhci_dev_t;

typedef struct xhci_endpoint {
    xhci_dev_t *dev;                        // Device of the endpoint
    xhci_transfer_ring_t *ring;             // Transfer ring for the endpoint
    USBEndpointDescriptor_t *desc;          // Endpoint descriptor
    uint8_t type;                           // Type of endpoint
    uint8_t num;                            // Number of endpoint
} xhci_endpoint_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize an xHCI controller
 * @param device The PCI device of the xHCI controller
 */
int xhci_initController(pci_device_t *device);

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


/**
 * @brief Poll the event ring for completion events
 * @param xhci The XHCI to poll
 */
void xhci_pollEventRing(xhci_t *xhci);

/**
 * @brief Hard reset an xHCI port
 * @param xhci The xHCI to reset the port on
 * @param port The port to reset
 * @returns 0 on success
 */
int xhci_portReset(xhci_t *xhci, int port);

/**
 * @brief Determine whether a port is USB3 or not
 * @param xhci The xHCI to use
 * @param port Zero-based port number to check
 * @returns 0 on USB2, 1 on USB3
 */
int xhci_portUSB3(xhci_t *xhci, int port);

#endif