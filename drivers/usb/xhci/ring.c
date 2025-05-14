/**
 * @file drivers/usb/xhci/ring.c
 * @brief xHCI ring handler
 * 
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

/* Update ERDP to new dequeue value */
#define ERDP_UPDATE(xhci) (EVENTRING(xhci)->regs->erdp = (uint64_t)(EVENTRING(xhci)->trb_list_phys + (EVENTRING(xhci)->dequeue * sizeof(xhci_trb_t))))

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

    // Create the first entry in the ERST
    EVENTRING(xhci)->erst[0].address = EVENTRING(xhci)->trb_list_phys;
    EVENTRING(xhci)->erst[0].size = XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t);
    EVENTRING(xhci)->erst[0].reserved = 0;

    // Configure the ERSTSZ register
    regs->erstsz = 1; // NOTE: Only one entry for now

    // Set the ERDP and ERSTBA
    ERDP_UPDATE(xhci);
    regs->erstba = mem_getPhysicalAddress(NULL, (uintptr_t)EVENTRING(xhci)->erst);
    LOG(DEBUG, "Event ring enabled (TRB list: %016llX)\n", regs->erstba);
    return 0;
}