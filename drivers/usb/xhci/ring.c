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
    memset(CMDRING(xhci)->trb_list, 0, XHCI_COMMAND_RING_TRB_COUNT * sizeof(xhci_trb_t));

    // Setup a link TRB
    // This will form a big chain around the command ring
    xhci_link_trb_t *link_trb = LINK_TRB(&CMDRING(xhci)->trb_list[XHCI_COMMAND_RING_TRB_COUNT-1]);
    link_trb->ring_segment = mem_getPhysicalAddress(NULL, (uintptr_t)CMDRING(xhci)->trb_list);
    link_trb->control.type = XHCI_TRB_TYPE_LINK;
    link_trb->control.tc = 1;
    link_trb->control.c = XHCI_CRCR_RING_CYCLE_STATE;

    // Configure in the CRCR
    xhci->opregs->crcr = mem_getPhysicalAddress(NULL, (uintptr_t)CMDRING(xhci)->trb_list);
    LOG(DEBUG, "Command ring enabled (%016llX)\n", xhci->opregs->crcr);
    return 0;
}