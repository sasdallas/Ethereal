/**
 * @file drivers/storage/ahci/ahci.c
 * @brief AHCI driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "ahci.h"
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <string.h>

// HAL
#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:AHCI", __VA_ARGS__)

/**
 * @brief AHCI scan method
 * @param data Pointer to PCI out
 */
int ahci_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    if (pci_readConfigOffset(bus, slot, function, PCI_PROGIF_OFFSET, 1) != 0x01) return 0; // Not an AHCI device

    
    // TODO: Check ven/dev id
    *(uint32_t*)data = PCI_ADDR(bus, slot, function, 0); 
    return 1;
}

/**
 * @brief AHCI interrupt controller
 * @param context Pointer to @c ahci_t structure
 */
int ahci_interrupt(void *context) {
    // Get the AHCI structure
    ahci_t *ahci = (ahci_t*)context;

    // LOG(DEBUG, "AHCI interrupt - IS %08x\n", ahci->mem->is);

    uint32_t is = ahci->mem->is;

    // For each port...
    for (int port = 0; port < 32; port++) {
        // Does it exist?
        if (ahci->ports[port] == NULL) continue;

        // Does it have an IRQ to handle?
        if (is & (1 << port)) {
            ahci_portIRQ(ahci->ports[port]);
        }
    }

    // Clear pending interrupts
    ahci->mem->is = is;

    return 0;
}

/**
 * @brief Probe AHCI device for ports
 * @param ahci Main AHCI controller
 * @returns 0 on success
 */
int ahci_probe(ahci_t *ahci) {
    // Get the number of ports and command slots
    int ports = ((ahci->mem->cap & HBA_CAP_NP));
    ahci->ncmdslot = ((ahci->mem->cap & HBA_CAP_NCS) >> HBA_CAP_NCS_SHIFT);

    // AHCI specification also says we need to use the PI register
    uint32_t pi = ahci->mem->pi;

    for (int i = 0; i < ports; i++) {
        if (pi & (1 << i)) {
            // If a port fails to initialize, it will return NULL. Second stage of
            // initialization will catch this and not ask the port to initialize again.
            ahci->ports[i] = ahci_portInitialize(ahci, i);
        }
    }

    return AHCI_SUCCESS;
}

/**
 * @brief Reset the AHCI controller
 * @returns AHCI_SUCCESS on success
 */
int ahci_resetController(ahci_t *ahci) {
    // Enable AHCI in the controller
    ahci->mem->ghc |= HBA_GHC_AE;

    // Reset the controller
    ahci->mem->ghc |= HBA_GHC_HR;

    // Wait until the controller is done resetting
    int reset = TIMEOUT(!(ahci->mem->ghc & HBA_GHC_HR), 1000000);
    if (reset) {
        LOG(ERR, "Controller timed out when resetting.\n");
        return AHCI_ERROR;
    } 

    return AHCI_SUCCESS;
}

/**
 * @brief AHCI init method
 */
int ahci_init(int argc, char **argv) {
    // Scan for AHCI controller
    uint32_t ahci_data = 0xFFFFFFFF;
    if (pci_scan(ahci_scan, (void*)&ahci_data, 0x0106) == 0) {
        // No AHCI controller
        LOG(INFO, "No AHCI controller found\n");
        return 0;
    }

    LOG(INFO, "Found AHCI controller at bus %d slot %d func %d\n", PCI_BUS(ahci_data), PCI_SLOT(ahci_data), PCI_FUNCTION(ahci_data));

    // Get ABAR (BAR5)
    pci_bar_t *bar = pci_readBAR(PCI_BUS(ahci_data), PCI_SLOT(ahci_data), PCI_FUNCTION(ahci_data), 5);

    // TODO: Has to be a MEMORY32? Could it be a MEMORY64?
    if (bar->type != PCI_BAR_MEMORY32) {
        LOG(ERR, "Invalid ABAR type %d. Aborting\n", bar->type);
        kfree(bar);
        return 1;
    }

    // Now we can configure the PCI command register.
    // Read it in and set flags
    uint16_t ahci_pci_command = pci_readConfigOffset(PCI_BUS(ahci_data), PCI_SLOT(ahci_data), PCI_FUNCTION(ahci_data), PCI_COMMAND_OFFSET, 2);
    ahci_pci_command &= ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_INTERRUPT_DISABLE); // Enable interrupts and disable I/O space
    ahci_pci_command |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE);
    pci_writeConfigOffset(PCI_BUS(ahci_data), PCI_SLOT(ahci_data), PCI_FUNCTION(ahci_data), PCI_COMMAND_OFFSET, (uint32_t)ahci_pci_command & 0xFFFF);

    // Map it into MMIO
    LOG(INFO, "HBA memory space location: %p\n", bar->address);
    ahci_hba_mem_t *hbamem = (ahci_hba_mem_t*)mem_mapMMIO(bar->address, bar->size);
    hbamem->ghc &= ~HBA_GHC_IE; // Disable interrupts

    // Construct the main AHCI controller structure
    ahci_t *ahci = kmalloc(sizeof(ahci_t));
    memset(ahci, 0, sizeof(ahci_t));
    ahci->mem = hbamem;
    ahci->pci_device = ahci_data;

    // Register the interrupt handler
    uint8_t irq = pci_getInterrupt(PCI_BUS(ahci->pci_device), PCI_SLOT(ahci->pci_device), PCI_FUNCTION(ahci->pci_device));

    // Does it have an IRQ?
    if (irq == 0xFF) {
        LOG(ERR, "AHCI controller does not have interrupt number\n");
        LOG(ERR, "This is an implementation bug, halting system (REPORT THIS)\n");
        for (;;);
    }


    LOG(DEBUG, "Registering IRQ%d for AHCI controller\n", irq);


    // Register a context-based interrupt handler?
    if (hal_registerInterruptHandlerContext(irq, ahci_interrupt, (void*)ahci) != 0) {
        LOG(ERR, "Error registering AHCI controller IRQ (I/O APIC in use?)\n");
        kfree(ahci);
        return 1;
    }


    // Reset the controller
    if (ahci_resetController(ahci)) {
        LOG(ERR, "Error initializing AHCI controller.\n");

        kfree(ahci); // !!!: Leaking memory
        return 1; // Nonfatal?
    }

    // If we're 64-bit, make sure the AHCI controller supports 64-bit addressing
#if INTPTR_MAX == INT64_MAX
    if (!(ahci->mem->cap & HBA_CAP_S64A)) {
        LOG(ERR, "AHCI controller does not support 64-bit addressing on 64-bit OS\n");
        LOG(ERR, "This is bypassable with a DMA buffer but this is not implemented\n");
        LOG(ERR, "Load failed. Please start an issue on GitHub.\n");
        kfree(ahci); // !!!: Leaking memory
        return 1;
    }
#endif

    // Debug
    uint32_t generation = ((hbamem->cap & HBA_CAP_ISS) >> HBA_CAP_ISS_SHIFT);
    LOG(DEBUG, "AHCI Controller: %s\n",
            (generation == 0x1) ? "Gen 1 (1.5 Gbps)" :
            (generation == 0x2) ? "Gen 2 (3 Gbps)" :
            (generation == 0x3) ? "Gen 3 (6 Gbps)" :
            "Unknown Generation");

    // Debug
    uint32_t version = hbamem->vs;
    LOG(DEBUG, "Controller version: %d.%d%d\n", (version >> 16) & 0xFFF, (version >> 8) & 0xFF, (version) & 0xFF);

    // Start probin'
    if (ahci_probe(ahci)) {
        LOG(ERR, "Error probing for ports.\n");
        return 1;
    }

    // Clear pending interrupts
    uint32_t is = ahci->mem->is;
    ahci->mem->is = is;

    // Enable interrupts
    ahci->mem->ghc |= HBA_GHC_IE;

    // Finish the port startup
    for (int port = 0; port < 32; port++) {
        if (ahci->ports[port]) {
            // TODO: Error handling, we're leaking memory with this
            ahci_portFinishInitialization(ahci->ports[port]);
        }
    }

    return 0;
}

/**
 * @brief AHCI deinit method
 */
int ahci_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "AHCI Driver",
    .author = "Samuel Stuart",
    .init = ahci_init,
    .deinit = ahci_deinit
};
