/**
 * @file drivers/usb/xhci_new/xhci.h
 * @brief xHCI driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _XHCI_H
#define _XHCI_H

/**** INCLUDES ****/
#include "xhci_definitions.h"
#include <kernel/drivers/usb2/usb.h>
#include <kernel/drivers/pci.h>
#include <kernel/task/process.h>
#include <kernel/misc/mutex.h>
#include <kernel/tasklet.h>
#include <structs/queue_rb.h>
#include <stdint.h>

/**** DEFINITIONS ****/

/* Default ring size */
#define XHCI_RING_SIZE (PAGE_SIZE/sizeof(xhci_trb_t))
#define XHCI_MAX_COMPLETIONS    100

/**** TYPES ****/

struct xhci;

typedef struct xhci_ring {
    mutex_t lock;
    xhci_trb_t *trb;
    uintptr_t trb_phys;
    bool cycle;
    unsigned int enqueue;
    unsigned int dequeue;
} xhci_ring_t;

struct xhci_bus;

typedef struct xhci_port {
    struct xhci_bus *parent;
    BITMAP_DEFINE(status_map, 16); // maps directly to official USB hub status
} xhci_port_t;

typedef struct xhci_bus {
    struct xhci *xhci;
    usb_bus_t *bus;

    spinlock_t lock;
    int port_base;
    int port_count;
    unsigned long *port_map;
    unsigned long *port_change_map;
    usb_transfer_t *rhub_pending;
} xhci_bus_t;

typedef enum xhci_completion_type {
    XHCI_COMPLETION_TRANSFER,
    XHCI_COMPLETION_PORT_CHANGE
} xhci_completion_type_t;

typedef struct xhci_completion {
    xhci_completion_type_t type;
    union {
        usb_transfer_t *transfer;
        
        struct {
            int id; // logical index in bus
            xhci_bus_t *bus;
        } port;
    };
} xhci_completion_t;

typedef struct xhci_pipe {
    struct xhci *xhci;
    xhci_ring_t *ring;

    // HACK: This queue is protected by the ring's lock!
    // TODO: Redo locking around here
    queue_rb_t transfers;
} xhci_pipe_t;

typedef struct xhci_device {
    struct xhci *xhci;
    uint8_t slot_id;

    void *device_context;
    void *input_context;

    unsigned char root_port;
    unsigned int route_string;

    xhci_pipe_t *pipes[32];

    int highest_ep;
} xhci_device_t;

typedef struct xhci {
    pci_device_t *pci;
    uintptr_t mmio;
    tasklet_t tasklet;
    usb_controller_t *controller;

    // Quick accesses to xHCI registers because it's good looking
    volatile xhci_cap_regs_t *caps;
    volatile xhci_op_regs_t *op;
    volatile xhci_runtime_regs_t *runtime;

    // Rings
    xhci_ring_t *cmd_ring;
    xhci_ring_t *event_ring;

    // Completions for completion thread + command TRB responses
    wait_queue_t completion_waiters;
    queue_obj_t completions;
    wait_queue_t command_waiters;
    queue_rb_t command_trbs; // list of pointers to waiting TRBs

    // TODO: lockless, it is possible but im too tired at the moment
    spinlock_t command_lock;
    spinlock_t completion_lock;

    xhci_device_t *devices[256];

    // Misc.
    uintptr_t *dcbaa;
    uintptr_t *scratchpad;
    uintptr_t erst;
} xhci_t;

/**** MACROS ****/

#define XHCI_PORTREGS(xhci, i) ((volatile xhci_port_regs_t*)&((xhci)->op->ports[i]))
#define XHCI_DOORBELL(xhci, i) (((volatile uint32_t*)((xhci)->mmio + (xhci)->caps->dboff))[(i)])

#define XHCI_CONTEXT_SIZE(xhci) ((xhci)->caps->hccparams1.context_size ? 64 : 32) 
#define XHCI_INPUT_CONTEXT(dev)                 ((xhci_input_context_t*)(dev->input_context))
#define XHCI_SLOT_CONTEXT(dev)                  ((xhci_slot_context_t*)((uintptr_t)dev->input_context + 1 * XHCI_CONTEXT_SIZE(xhci)))
#define XHCI_ENDPOINT_CONTEXT(dev, epid)        ((xhci_endpoint_context_t*)((uintptr_t)dev->input_context + ((epid)+1) * XHCI_CONTEXT_SIZE(xhci)))
#define XHCI_OUTPUT_SLOT_CONTEXT(dev)           ((xhci_slot_context_t*)(dev->device_context))
#define XHCI_OUTPUT_ENDPOINT_CONTEXT(dev, epid) ((xhci_endpoint_context_t*)((uintptr_t)dev->device_context + (epid) * XHCI_CONTEXT_SIZE(xhci)))

#define XHCI_LOCK_RING(ring) mutex_acquire(&(ring)->lock)
#define XHCI_UNLOCK_RING(ring) mutex_release(&(ring)->lock)

/**** FUNCTIONS ****/

/* xHCI */
int xhci_sendCommand(xhci_t *xhci, void *trb, xhci_command_completion_trb_t *trbout);

/* xHCI ring */
xhci_ring_t *xhci_createRing();
xhci_trb_t *xhci_dequeueRing(xhci_ring_t *ring);
void xhci_enqueueRing(xhci_ring_t *ring, xhci_trb_t *trb);
void xhci_freeRing(xhci_ring_t *ring);

/* xHCI pipe */
extern usb_pipe_ops_t xhci_control_ep_ops;
extern usb_pipe_ops_t xhci_intr_ep_ops;
usb_status_t xhci_configurePipe(xhci_t *xhci, usb_pipe_t *pipe);
void xhci_freePipe(xhci_t *xhci, usb_pipe_t *pipe);

/* xHCI roothub */
extern usb_pipe_ops_t xhci_roothub_intr_ops;
usb_status_t xhci_root_hub_control(usb_bus_t *bus, usb_transfer_t *transfer);

static inline usb_status_t xhci_convertTRBStatus(uint8_t cc) {
    switch (cc) {
        case XHCI_TRB_STATUS_SUCCESS:
        case XHCI_TRB_STATUS_SHORT_PACKET:
            return USB_SUCCESS;

        case XHCI_TRB_STATUS_STALL:
            return USB_STALLED;

        default:
            // TODO
            return USB_INTERNAL_ERROR;
    };
}

static inline int xhci_getDCI(usb_endpoint_t *endp) {
    uint8_t ep = (endp->desc.bEndpointAddress) & 0x0F;
    if (ep == 0) return 1;
    return ((ep * 2) + (((endp->desc.bEndpointAddress) & USB_ENDP_DIR_IN) ? 1 : 0));
}

#endif
