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

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "HAL"
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:CON ] "); \
                            dprintf(NOHEADER, __VA_ARGS__);

/**
 * @brief Reset an xHCI controller
 * @param xhci The controller that needs to be reset
 * @returns 0 on success
 */
static int xhci_resetController(xhci_t *xhci) {
    // To reset the xHCI controller, we have to:
    // 1. Clear the R/S bit in USBCMD
    // 2. Wait for HCHalted to set
    // 3. Set HCRST
    // 4. Wait for HCRST to clear

    // Clear the R/S bit
    xhci->opregs->usbcmd &= ~(XHCI_USBCMD_RUN_STOP);

    // Wait for HCHalted to set 
    // if (XHCI_TIMEOUT((xhci->opregs->usbsts & XHCI_USBSTS_HCH), 200)) {
    //     LOG(ERR, "Controller reset failed: HCHalted timeout expired\n");
    //     return 1;
    // }

    // TEMPORARY
    while (1) {
        uint32_t usbsts = xhci->opregs->usbsts;
        if (usbsts & XHCI_USBSTS_HCH) {
            break;
        }

        LOG(DEBUG, "Waiting on HCH: %x\n", usbsts);
    }

    // Set HCRST
    xhci->opregs->usbcmd |= XHCI_USBCMD_HCRESET;

    // Wait for HCRESET to clear
    // if (XHCI_TIMEOUT(!(xhci->opregs->usbcmd & XHCI_USBCMD_HCRESET), 1000)) {
        // LOG(ERR, "Controller reset failed: Timeout expired while waiting for completion\n");
        // return 1;
    // }

    // TEMPORARY
    while (1) {
        uint32_t usbcmd = xhci->opregs->usbcmd;
        if (!(usbcmd & XHCI_USBCMD_HCRESET)) break;

        LOG(DEBUG, "Waiting on HCRESET: %x\n", usbcmd);
    }

    LOG(INFO, "Reset controller success\n");;
    return 0;
}

/**
 * @brief Acknowledge an IRQ released by a controller
 * @param xhci The controller to acknowledge
 * @param interrupter The interrupter to acknowledge
 */
void xhci_acknowledge(xhci_t *xhci, int interrupter) {
    // Get the interrupter's IMAN
    uint32_t iman = xhci->runtime->ir[interrupter].iman;

    // Write the IP bit
    iman |= XHCI_IMAN_INTERRUPT_PENDING;
    xhci->runtime->ir[interrupter].iman = iman;

    // Clear EINT bit by writing a one to it
    xhci->opregs->usbsts |= XHCI_USBSTS_EINT;
}

/**
 * @brief xHCI IRQ handler
 * @param context The xHCI controller
 */
int xhci_irqHandler(void *context) {
    xhci_t *xhci = (xhci_t*)context;
    if (xhci) {
        LOG(DEBUG, "xHCI IRQ\n");
        xhci_acknowledge(xhci, 0);
    }

 
    return 0;
}

/**
 * @brief Start xHCI controller
 * @param xhci The controller to start
 */
int xhci_startController(xhci_t *xhci) {
    // To start the controller, we need to set R/S, IE, and HOSTSYS_EE
    xhci->opregs->usbcmd |= XHCI_USBCMD_RUN_STOP | XHCI_USBCMD_INTERRUPTER_ENABLE | XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
    
    // Wait
    while (1) {
        uint32_t usbsts = xhci->opregs->usbsts;
        if (!(usbsts & XHCI_USBSTS_HCH)) break;

        LOG(DEBUG, "Waiting on HCH: %x\n", usbsts);
    }


    // If CNR is still set (Controller Not Ready) then we failed
    if (xhci->opregs->usbsts & XHCI_USBSTS_CNR) {
        LOG(ERR, "CNR still set: Start failed\n");
        return 1;
    }

    return 0;
}

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

    // Map it in
    LOG(DEBUG, "MMIO map: size 0x%016llX addr 0x%016llX bar type %d\n", bar->size, bar->address, bar->type);
    xhci->mmio_addr = mem_mapMMIO(bar->address, bar->size);

    // Get some registers
    xhci->capregs = (xhci_cap_regs_t*)xhci->mmio_addr;
    xhci->opregs = (xhci_op_regs_t*)(xhci->mmio_addr + xhci->capregs->caplength);
    xhci->runtime = (xhci_runtime_regs_t*)(xhci->mmio_addr + xhci->capregs->rtsoff);

    LOG(DEBUG, "This controller supports up to %d devices, %d interrupts, and has %d ports\n", XHCI_MAX_DEVICE_SLOTS(xhci->capregs), XHCI_MAX_INTERRUPTERS(xhci->capregs), XHCI_MAX_PORTS(xhci->capregs));

    // Reset the controller
    if (xhci_resetController(xhci)) {
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        return 1;
    } 

    // Enable device notifications
    xhci->opregs->dnctrl = 0xFFFF;

    // Program the max number of slots
    uint32_t config = xhci->opregs->config;
    config &= ~0xFF;
    config |= XHCI_MAX_DEVICE_SLOTS(xhci->capregs);
    xhci->opregs->config = config;

    // Now, let's setup the DCBAA (Device Context Base Address Array)
    size_t dcbaa_size = sizeof(xhci_dcbaa_t) * (XHCI_MAX_DEVICE_SLOTS(xhci->capregs)-1);
    xhci->dcbaa = (xhci_dcbaa_t*)mem_allocateDMA(dcbaa_size);
    xhci->dcbaa_virt = kzalloc(dcbaa_size);

    // Zero DMA
    memset(xhci->dcbaa, 0, sizeof(xhci_dcbaa_t) * (XHCI_MAX_DEVICE_SLOTS(xhci->capregs)-1));

    // Setup scratchpad buffers
    if (XHCI_MAX_SCRATCHPAD_BUFFERS(xhci->capregs)) {
        // xHCI spec says we need to allocate a scratchpad array as the 0th entry in the DCBAA.
        // Allocate memory for the scratchpad array
        // !!!: Waste of memory.. this probably uses a whole page. We can maybe fit this in with the DCBAA?
        uintptr_t *scratchpad = (uintptr_t*)mem_allocateDMA(XHCI_MAX_SCRATCHPAD_BUFFERS(xhci->capregs) * sizeof(uintptr_t*));
        memset(scratchpad, 0, XHCI_MAX_SCRATCHPAD_BUFFERS(xhci->capregs) * sizeof(uintptr_t*));

        // Allocate scratchpad pages
        for (size_t i = 0; i < XHCI_MAX_SCRATCHPAD_BUFFERS(xhci->capregs); i++) {
            uintptr_t page = mem_allocateDMA(PAGE_SIZE);
            scratchpad[i] = mem_getPhysicalAddress(NULL, page);
        }

        // Configure it in the DCBAA
        xhci->dcbaa[0] = mem_getPhysicalAddress(NULL, (uintptr_t)scratchpad);
        xhci->dcbaa_virt[0] = (xhci_dcbaa_t)scratchpad;
    }

    // Program it in
    xhci->opregs->dcbaap = mem_getPhysicalAddress(NULL, (uintptr_t)xhci->dcbaa);
    LOG(DEBUG, "DCBAA: %016llX (entry 0/scratchpad array: %016llX)\n", xhci->opregs->dcbaap, xhci->dcbaa_virt[0]);

    // Enable the command ring
    if (xhci_initializeCommandRing(xhci)) {
        LOG(ERR, "Error while initializing command ring\n");
        
        // TODO: Free scratcpad
        mem_freeDMA((uintptr_t)xhci->dcbaa, dcbaa_size);
        kfree(xhci->dcbaa_virt);
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        return 1;
    }

    // Enable IRQs
    xhci->runtime->ir[0].iman |= XHCI_IMAN_INTERRUPT_ENABLE;
    
    // Clear pending
    xhci_acknowledge(xhci, 0);

    // Register IRQ handler
    // TODO: MSI(-X) needs to be worked out and/or I/O APIC. This is a bad hack.
    uintptr_t irq = pci_getInterrupt(PCI_BUS(device), PCI_SLOT(device), PCI_FUNCTION(device));
    hal_unregisterInterruptHandler(irq);
    if (hal_registerInterruptHandlerContext(irq, xhci_irqHandler, (void*)xhci)) {
        LOG(ERR, "Error while registering IRQ%d\n", irq);
        return 1;
    }

    // Start the controller
    if (xhci_startController(xhci)) {
        LOG(ERR, "Error while starting controller\n");
        
        // TODO: Free scratchpad
        mem_freeDMA((uintptr_t)xhci->dcbaa, dcbaa_size);
        kfree(xhci->dcbaa_virt);
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        return 1;
    }

    // Start probing
    LOG(INFO, "xHCI controller started successfully\n");
    for (uint8_t i = 0; i < 8; i++) {
        xhci_port_registers_t *port = (xhci_port_registers_t*)XHCI_PORT_REGISTERS(xhci->opregs, i);

        if (port->portsc & XHCI_PORTSC_CCS && port->portsc & XHCI_PORTSC_CSC) {
            LOG(DEBUG, "xHCI USB device detected on port %d\n", i);
            xhci_portInitialize(xhci, i);
        }
    }
     
    return 0;
}