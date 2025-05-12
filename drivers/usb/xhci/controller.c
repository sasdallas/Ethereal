/**
 * @file drivers/usb/xhci/controller.c
 * @brief xHCI controller handler
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
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/drivers/pci.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:CON ] "); \
                            dprintf(NOHEADER, __VA_ARGS__);

/**
 * @brief Initialize an xHCI controller
 * @param device The PCI device of the xHCI controller
 */
int xhci_initController(uint32_t device) {
    LOG(INFO, "Initializing xHCI controller (bus %d, slot %d, function %d)\n", PCI_BUS(device), PCI_SLOT(device), PCI_FUNCTION(device));

    // Start by making a new xhci_t structure
    xhci_t *xhci = kzalloc(sizeof(xhci_t));
    xhci->pci_addr = device;
    
    // Read in BAR0
    pci_bar_t *bar = pci_readBAR(PCI_BUS(device), PCI_SLOT(device), PCI_FUNCTION(device), 0);
    if (!bar) {
        LOG(ERR, "xHCI does not have BAR0\n");
        kfree(xhci);
        return 1;
    }

    if (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64) {
        // BAR is not valid type
        LOG(ERR, "xHCI BAR0 is of unrecognized type %d\n", bar->type);
        kfree(xhci);
        return 1;
    }

    // !!!: Definitely a bug in the PCI system... These have bad bits set.
    uintptr_t address = (uint32_t)bar->address;
    size_t size = (uint32_t)bar->size;
    LOG(DEBUG, "MMIO map: size 0x%016llX addr 0x%016llX bar type %d\n", size, address, bar->type);
    xhci->mmio_addr = mem_mapMMIO(address, size);

    // Get some capability information
    xhci->capregs = (xhci_cap_regs_t*)xhci->mmio_addr;

    LOG(DEBUG, "This controller supports up to %d devices, %d interrupts, and has %d ports\n", XHCI_MAX_DEVICE_SLOTS(xhci->capregs), XHCI_MAX_INTERRUPTERS(xhci->capregs), XHCI_MAX_PORTS(xhci->capregs));


    return 1;
}