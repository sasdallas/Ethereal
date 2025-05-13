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
#include "xhci_trb.h"
#include <kernel/misc/spinlock.h>

/**** TYPES ****/

typedef struct xhci_cmd_ring {
    spinlock_t *lock;               // Lock
    xhci_trb_t *trb_list;           // TRB list
} xhci_cmd_ring_t;

/**** MACROS ****/

#define CMDRING(xhci) (xhci->cmd_ring)
#define LINK_TRB(trb) ((xhci_link_trb_t*)(trb))

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
int xhci_insertTRB(struct xhci *xhci, xhci_trb_t *trb);


#endif