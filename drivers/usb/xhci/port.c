/**
 * @file drivers/usb/xhci/port.c
 * @brief xHCI port handler
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

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "HAL"
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:PORT] "); \
                            dprintf(NOHEADER, __VA_ARGS__);


/**
 * @brief Hard reset an xHCI port
 * @param xhci The xHCI to reset the port on
 * @param port The port to reset
 * @returns 0 on success
 */
int xhci_portReset(xhci_t *xhci, int port) {
    // Get the PORTSC register
    xhci_port_registers_t *pr = XHCI_PORT_REGISTERS(xhci->opregs, port);

    // Set the port reset bit
    pr->portsc |= XHCI_PORTSC_PR;

    // Wait for the port status change event
    if (XHCI_TIMEOUT((pr->portsc & XHCI_PORTSC_PRC), 1000)) {
        LOG(ERR, "Failed to reset port %d - timeout while waiting for PRC to set\n");
        return 1;
    }

    return 0;
}


/**
 * @brief Try to initialize an xHCI port
 * @param xhci The xHCI device to use
 * @param port The port number to attempt to initialize
 * @returns 0 on success
 */
int xhci_portInitialize(xhci_t *xhci, int port) {
    // First, try to perform a port reset (xHCI spec 4.3.1)
    if (xhci_portReset(xhci, port)) {
        LOG(ERR, "Initialization of port %d failed\n", port);
        return 1;
    }

    LOG(DEBUG, "Port %d reset successfully\n", port);
    
    return 0;
}