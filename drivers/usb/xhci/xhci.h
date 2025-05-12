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

/**** TYPES ****/

typedef struct xhci {
    uint32_t pci_addr;          // PCI address in case any new calls need to be made
    uintptr_t mmio_addr;        // MMIO address
    xhci_cap_regs_t *capregs;   // Capability registers
    xhci_op_regs_t *opregs;     // Operational registers
    
} xhci_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize an xHCI controller
 * @param device The PCI device of the xHCI controller
 */
int xhci_initController(uint32_t device);


#endif