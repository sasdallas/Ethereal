/**
 * @file drivers/usb/xhci/xhci_ring.h
 * @brief Command and event ring
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _XHCI_RING_H
#define _XHCI_RING_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include "xhci_trb.h"
#include "xhci_regs.h"
#include <kernel/misc/spinlock.h>

/**** TYPES ****/

typedef struct xhci_cmd_ring {
    spinlock_t *lock;               // Lock
    xhci_trb_t *trb_list;           // TRB list
    uint32_t enqueue;               // Enqueue pointer
    uint8_t cycle;                  // Cycle
} xhci_cmd_ring_t;

// Event Ring Segment Table entry
typedef struct xhci_erst_entry {
    uint64_t address;               // Base
    uint32_t size;                  // Size
    uint32_t reserved;              // Reserved
} __attribute__((packed)) xhci_erst_entry_t;    

typedef struct xhci_event_ring {
    xhci_int_regs_t *regs;          // Interrupter regs
    xhci_trb_t *trb_list;           // TRB list
    uintptr_t trb_list_phys;        // Physical TRB list address (cached)
    xhci_erst_entry_t *erst;        // Event ring segment table
    uint32_t dequeue;               // Dequeue pointer
    uint8_t cycle;                  // Cycle bit
} xhci_event_ring_t;

/**** MACROS ****/

#define CMDRING(xhci) (xhci->cmd_ring)
#define EVENTRING(xhci) (xhci->event_ring)
#define LINK_TRB(trb) ((xhci_link_trb_t*)(trb))

#define XHCI_EVENT_RING_DEQUEUE(xhci) (EVENTRING(xhci)->trb_list[EVENTRING(xhci)->dequeue])
#define XHCI_EVENT_RING_AVAILABLE(xhci) (XHCI_EVENT_RING_DEQUEUE(xhci).control.c == EVENTRING(xhci)->cycle)

// !!!: I don't know about this.. -1 on dequeue? qemu breaks otherwise
#define ERDP_UPDATE(xhci) (EVENTRING(xhci)->regs->erdp = (uint64_t)(EVENTRING(xhci)->trb_list_phys + ((EVENTRING(xhci)->dequeue-1) * sizeof(xhci_trb_t))))

/**** FUNCTIONS ****/

struct xhci;

/**
 * @brief Initialize the xHCI command ring for a specific controller
 * @param xhci The controller to initialize for
 * @returns 0 on success
 */
int xhci_initializeCommandRing(struct xhci *xhci);

/**
 * @brief Deinitialize the xHCI command ring
 * @param xhci The controller to deinitialize the command ring for
 * @returns 0 on success
 */
int xhci_deinitializeCommandRing(struct xhci *xhci);

/**
 * @brief Insert a new TRB into the command ring
 * @param xhci The controller to insert the TRB into
 * @param trb The TRB to insert into the command ring
 * @returns 0 on success
 */
int xhci_enqueueTRB(struct xhci *xhci, xhci_trb_t *trb);

/**
 * @brief Initialize the xHCI event ring for a specific interrupter on a specific controller
 * @param xhci The controller to initialize an event ring for
 */
int xhci_initializeEventRing(struct xhci *xhci);

/**
 * @brief Pop a TRB from the event ring
 * @param xhci The controller to pop the TRB from
 * @returns TRB on success, NULL on failure
 */
xhci_trb_t *xhci_dequeueTRB(struct xhci *xhci);

#endif