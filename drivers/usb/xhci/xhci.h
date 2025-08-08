/**
 * @file drivers/usb/xhci/xhci.h
 * @brief Generic xHCI controller definitions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef XHCI_H
#define XHCI_H

/**** INCLUDES ****/
#include "xhci_definitions.h"
#include <kernel/task/process.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/misc/mutex.h>
#include <stdatomic.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

struct xhci;

typedef struct xhci_transfer_ring {
    xhci_trb_t *trb_list;           // TRB list
    uint32_t enqueue;               // Enqueue
    uint32_t dequeue;               // Dequeue
    uint8_t cycle;                  // Cycle
    uintptr_t trb_list_phys;        // Physical address of TRB list
} xhci_transfer_ring_t;

typedef struct xhci_endpoint {
    xhci_transfer_ring_t *tr;       // Transfer ring
    uint32_t mps;                   // MPS
    mutex_t *m;                     // Mutex
    
    // HACKY
    xhci_transfer_completion_trb_t ctr;
    volatile uint8_t flag;          // Flag
} xhci_endpoint_t;


typedef struct xhci_device {
    mutex_t *mutex;                         // Mutex
    struct xhci *parent;                    // xHCI controller
    uint8_t port_id;                        // Port ID
    uint8_t slot_id;                        // xHCI slot ID

    xhci_input_context_t *input_ctx;        // Input context
    uintptr_t input_ctx_phys;               // Physical address of input context

    uintptr_t output_ctx;                   // Output context

    USBDevice_t *dev;                       // USB device
    xhci_endpoint_t endpoints[32];          // Endpoints
} xhci_device_t;

typedef struct xhci_cmd_ring {
    xhci_trb_t *trb_list;           // TRB list
    uint32_t enqueue;               // Enqueue pointer
    uint8_t cycle;                  // Cycle
} xhci_cmd_ring_t;

typedef struct xhci_event_ring {
    xhci_trb_t *trb_list;           // TRB list
    uint32_t dequeue;               // Dequeue
    xhci_event_ring_entry_t *ent;   // The only entry in the event ring
    uint8_t cycle;                  // Cycle state
    uintptr_t trb_list_phys;        // Physical address of TRB list
} xhci_event_ring_t;

typedef struct xhci_port_info {
    uint8_t rev_major;              // Major revision
    uint8_t rev_minor;              // Minor revision
    uint8_t slot_id;                // Slot ID for port (if set, the port has been enabled)
} xhci_port_info_t;

typedef struct xhci {
    pci_device_t *dev;                              // PCI device
    uintptr_t mmio_addr;                            // MMIO address

    // REGISTERS
    volatile xhci_cap_regs_t *cap;                  // Capability registers
    volatile xhci_op_regs_t *op;                    // Operational registers
    volatile xhci_runtime_regs_t *run;              // Runtime registers

    // RINGS
    xhci_cmd_ring_t *cmd_ring;                      // Command ring
    xhci_event_ring_t *event_ring;                  // Event ring

    // REGIONS
    uintptr_t dcbaa;                                // DCBAA
    uintptr_t scratchpad;                           // Scratchpad

    // OTHER
    mutex_t *mutex;                                 // Mutex
    xhci_port_info_t *ports;                        // Port information list
    xhci_device_t **slots;                          // Slot list
    process_t *poller;                              // Port poller thread 
    USBController_t *controller;                    // controller              

    // ctrb + friends garbage
    xhci_command_completion_trb_t ctr;              // Completion TRB
    volatile uint8_t flag;                          // Flag
    volatile uint8_t port_status_changed;           // Port status was changed, keep iterating                            // MSI in use
} xhci_t;

/**** VARIABLES ****/

extern int xhci_controller_count;

/**** MACROS ****/

#define XHCI_EVENT_RING_DEQUEUE(xhci) (&((xhci)->event_ring->trb_list[(xhci)->event_ring->dequeue]))
#define XHCI_ACKNOWLEDGE(xhci) (xhci)->run->irs[0].iman = (xhci)->run->irs[0].iman | XHCI_IMAN_INTERRUPT_ENABLE | XHCI_IMAN_INTERRUPT_PENDING

#define XHCI_DOORBELL(xhci, i) (((volatile uint32_t*)((xhci)->mmio_addr + (xhci)->cap->dboff))[(i)])

#define XHCI_CONTEXT_SIZE(xhci) ((xhci)->cap->context_size ? 64 : 32) 

#define XHCI_INPUT_CONTEXT(dev)                 ((xhci_input_context_t*)(dev->input_ctx))
#define XHCI_SLOT_CONTEXT(dev)                  ((xhci_slot_context_t*)((uintptr_t)dev->input_ctx + 1 * XHCI_CONTEXT_SIZE(dev->parent)))
#define XHCI_ENDPOINT_CONTEXT(dev, epid)        ((xhci_endpoint_context_t*)((uintptr_t)dev->input_ctx + ((epid)+1) * XHCI_CONTEXT_SIZE(dev->parent)))

/**** FUNCTIONS ****/

/**
 * @brief Initialize xHCI controller
 * @param device The PCI device
 */
int xhci_initController(pci_device_t *device);

/**
 * @brief Dequeue event TRB
 * @param xhci The xHCI controller
 * @returns TRB on success or NULL
 */
xhci_trb_t *xhci_dequeueEventTRB(xhci_t *xhci);

/**
 * @brief Initialize a device
 * @param xhci The xHCI controller
 * @param port The port number of the device
 * @returns 0 on success
 */
int xhci_initializeDevice(xhci_t *xhci, uint8_t port);

/**
 * @brief Send a command to the xHCI controller
 * @param xhci The xHCI controller
 * @param trb The command TRB
 * @returns TRB on success, NULL is an error
 */
xhci_command_completion_trb_t* xhci_sendCommand(xhci_t *xhci, xhci_trb_t *trb);

#endif