/**
 * @file drivers/usb/xhci/ring.c
 * @brief xHCI ring handler
 * 
 * Command and event rings are abstracted in the xHCI structure
 * Transfer rings are created structures that the caller keeps track of
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "xhci.h"
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:RING] "); \
                            dprintf(NOHEADER, __VA_ARGS__);


/**
 * @brief Initialize the xHCI command ring for a specific controller
 * @param xhci The controller to initialize for
 * @returns 0 on success
 */
int xhci_initializeCommandRing(xhci_t *xhci) {
    // Create the controller memory block
    CMDRING(xhci) = kzalloc(sizeof(xhci_cmd_ring_t));
    CMDRING(xhci)->lock = spinlock_create("xhci command ring lock");
    CMDRING(xhci)->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_COMMAND_RING_TRB_COUNT * sizeof(xhci_trb_t));
    CMDRING(xhci)->cycle = XHCI_CRCR_RING_CYCLE_STATE;

    memset(CMDRING(xhci)->trb_list, 0, XHCI_COMMAND_RING_TRB_COUNT * sizeof(xhci_trb_t));

    // Setup a link TRB
    // This will form a big chain around the command ring
    xhci_link_trb_t *link_trb = LINK_TRB(&CMDRING(xhci)->trb_list[XHCI_COMMAND_RING_TRB_COUNT-1]);
    link_trb->ring_segment = mem_getPhysicalAddress(NULL, (uintptr_t)CMDRING(xhci)->trb_list);
    link_trb->control.type = XHCI_TRB_TYPE_LINK;
    link_trb->control.tc = 1;
    link_trb->control.c = XHCI_CRCR_RING_CYCLE_STATE;

    // Configure in the CRCR
    xhci->opregs->crcr = mem_getPhysicalAddress(NULL, (uintptr_t)CMDRING(xhci)->trb_list) | CMDRING(xhci)->cycle;
    LOG(DEBUG, "Command ring enabled (%016llX)\n", xhci->opregs->crcr);
    return 0;
}

/**
 * @brief Insert a new TRB into the command ring
 * @param xhci The controller to insert the TRB into
 * @param trb The TRB to insert into the command ring
 * @returns 0 on success
 */
int xhci_enqueueTRB(struct xhci *xhci, xhci_trb_t *trb) {
    spinlock_acquire(CMDRING(xhci)->lock);

    // Set the TRB cycle bit
    trb->control.c = CMDRING(xhci)->cycle;

    // Now queue it
    CMDRING(xhci)->trb_list[CMDRING(xhci)->enqueue] = *trb;

    // Increment enqueue
    CMDRING(xhci)->enqueue++;
    if (CMDRING(xhci)->enqueue >= XHCI_COMMAND_RING_TRB_COUNT-1) {
        // We need to wrap it around and update the link TRB
        xhci_link_trb_t *link_trb = LINK_TRB(&CMDRING(xhci)->trb_list[XHCI_COMMAND_RING_TRB_COUNT-1]);
        link_trb->control.type = XHCI_TRB_TYPE_LINK;
        link_trb->control.tc = 1;
        link_trb->control.c = CMDRING(xhci)->cycle;

        CMDRING(xhci)->enqueue = 0;
        CMDRING(xhci)->cycle ^= 1;
    }

    spinlock_release(CMDRING(xhci)->lock);
    return 0;
}


/**
 * @brief Initialize the xHCI event ring for a specific interrupter on a specific controller
 * @param xhci The controller to initialize an event ring for
 */
int xhci_initializeEventRing(struct xhci *xhci) {
    // We want to add the event ring to interrupter #0
    xhci_int_regs_t *regs = &xhci->runtime->ir[0];
    
    // Allocate the event ring
    EVENTRING(xhci) = kzalloc(sizeof(xhci_event_ring_t));
    EVENTRING(xhci)->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t));
    EVENTRING(xhci)->erst = (xhci_erst_entry_t*)mem_allocateDMA(sizeof(xhci_erst_entry_t)); // For now we are only gonna use one entry, since that's required
    EVENTRING(xhci)->cycle = XHCI_CRCR_RING_CYCLE_STATE;
    EVENTRING(xhci)->regs = regs;
    EVENTRING(xhci)->trb_list_phys = mem_getPhysicalAddress(NULL, (uintptr_t)EVENTRING(xhci)->trb_list);

    memset(EVENTRING(xhci)->trb_list, 0, XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t));
    memset(EVENTRING(xhci)->erst, 0, PAGE_SIZE);

    // Create the first entry in the ERST
    EVENTRING(xhci)->erst[0].address = EVENTRING(xhci)->trb_list_phys;
    EVENTRING(xhci)->erst[0].size = XHCI_EVENT_RING_TRB_COUNT;
    EVENTRING(xhci)->erst[0].reserved = 0;

    // Configure the ERSTSZ register
    regs->erstsz = 1; // NOTE: Only one entry for now

    // Set the ERDP and ERSTBA
    regs->erdp = EVENTRING(xhci)->trb_list_phys;
    regs->erstba = mem_getPhysicalAddress(NULL, (uintptr_t)EVENTRING(xhci)->erst);
    LOG(DEBUG, "Event ring enabled (TRB list: %016llX)\n", regs->erstba);
    return 0;
}

/**
 * @brief Pop a TRB from the event ring
 * @param xhci The controller to pop the TRB from
 * @returns TRB on success, NULL on failure
 */
xhci_trb_t *xhci_dequeueTRB(struct xhci *xhci) {
    if (!XHCI_EVENT_RING_AVAILABLE(xhci)) return NULL;

    xhci_trb_t *trb = &XHCI_EVENT_RING_DEQUEUE(xhci);

    // Wrap dequeue around
    EVENTRING(xhci)->dequeue++;
    if (EVENTRING(xhci)->dequeue >= XHCI_EVENT_RING_TRB_COUNT) {
        EVENTRING(xhci)->dequeue = 0;
        EVENTRING(xhci)->cycle ^= 1;
    }

    return trb;
}

/**
 * @brief Create a new xHCI transfer ring
 * @returns A new xHCI transfer ring
 */
xhci_transfer_ring_t *xhci_createTransferRing() {
    xhci_transfer_ring_t *ring = kzalloc(sizeof(xhci_transfer_ring_t));
    ring->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_TRANSFER_RING_TRB_COUNT * sizeof(xhci_trb_t));
    ring->cycle = 1;
    ring->lock = spinlock_create("xhci transfer ring lock");
    ring->enqueue = 0;

    memset(ring->trb_list, 0, XHCI_TRANSFER_RING_TRB_COUNT * sizeof(xhci_trb_t));
    
    // Setup link TRB
    // This will form a big chain around the command ring
    xhci_link_trb_t *link_trb = LINK_TRB(&ring->trb_list[XHCI_TRANSFER_RING_TRB_COUNT-1]);
    link_trb->ring_segment = mem_getPhysicalAddress(NULL, (uintptr_t)ring->trb_list);
    link_trb->control.type = XHCI_TRB_TYPE_LINK;
    link_trb->control.tc = 1;
    link_trb->control.c = ring->cycle;

    return ring;
}

/**
 * @brief Insert a TRB into an xHCI transfer ring
 * @param ring The ring to insert the TRB into
 * @param trb The TRB to insert into the ring
 * @returns 0 on success
 */
int xhci_enqueueTransferTRB(xhci_transfer_ring_t *ring, xhci_trb_t *trb) {
    LOG(DEBUG, "Enqueue TRB %p to ring %p (current cycle bit: %d, enqueue: %d)\n", trb, ring, ring->cycle, ring->enqueue);
    spinlock_acquire(ring->lock);
    
    // Fix enqueue
    if (ring->enqueue >= XHCI_TRANSFER_RING_TRB_COUNT-1) {
        // Syncronize link TRB and invert cycle
        xhci_link_trb_t *link_trb = LINK_TRB(&ring->trb_list[XHCI_TRANSFER_RING_TRB_COUNT-1]);
        link_trb->control.type = XHCI_TRB_TYPE_LINK;
        link_trb->control.tc = 1;
        link_trb->control.c = ring->cycle;
    
        // Wrap
        ring->enqueue = 0;
        ring->cycle ^= 1;
    }

    LOG(DEBUG, "\tTRB will be enqueued to %p\n", mem_getPhysicalAddress(NULL, (uintptr_t)&ring->trb_list[ring->enqueue]));

    // Setup cycle bit
    trb->control.c = ring->cycle;

    // Enqueue
    ring->trb_list[ring->enqueue] = *trb;

    // Update enqueue
    ring->enqueue++;

    spinlock_release(ring->lock);
    return 0;
}