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
#include <kernel/arch/arch.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/drivers/pci.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/misc/args.h>

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "HAL"
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:CON ] " __VA_ARGS__);

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
    // Clear EINT bit by writing a one to it
    xhci->opregs->usbsts = XHCI_USBSTS_EINT;
    
    // Get the interrupter's IMAN
    uint32_t iman = xhci->runtime->ir[interrupter].iman;

    // Write the IP bit
    iman |= XHCI_IMAN_INTERRUPT_PENDING | XHCI_IMAN_INTERRUPT_ENABLE;
    xhci->runtime->ir[interrupter].iman = iman;
}

/**
 * @brief Determine whether a port is USB3 or not
 * @param xhci The xHCI to use
 * @param port Zero-based port number to check
 * @returns 0 on USB2, 1 on USB3
 */
int xhci_portUSB3(xhci_t *xhci, int port) {
    // TODO: This requires parsing the extended capabilities, which is broken right now
    return 0;
}

/**
 * @brief Poll the event ring for completion events
 * @param xhci The XHCI to poll
 */
void xhci_pollEventRing(xhci_t *xhci) {
    // Start reading
    while (XHCI_EVENT_RING_AVAILABLE(xhci)) {
        // !!!: probably wasteful and stupid
        xhci_trb_t *trb = xhci_dequeueTRB(xhci);
        if (!trb) {
            LOG(ERR, "Could not get TRB\n");
            return;
        };

        LOG(DEBUG, "Dequeued TRB: type=%d parameter=%p\n", trb->control.type, trb->specific);
        // Check to see if the TRB is a oprt status change event
        if (trb->control.type == XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT) {
            // We need to handle this one
            xhci_port_status_change_trb_t *port_trb = (xhci_port_status_change_trb_t*)trb;
            uint8_t port_num = port_trb->port_id - 1;
            LOG(INFO, "Port status change detected for port %d\n", port_num);

            // xhci_portInitialize(xhci, port_num);
            list_append(xhci->port_queue, (void*)port_trb);
        }
        
        if (trb->control.type == XHCI_TRB_TYPE_CMD_COMPLETION_EVENT) {
            // Add to command completion queue
            xhci_command_completion_trb_t *cc = (xhci_command_completion_trb_t*)trb;
            LOG(INFO, "An xHCI command was completed with completion code %d\n", cc->completion_code);
            list_append(xhci->command_queue, (void*)trb);
        } else if (trb->control.type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
            list_append(xhci->transfer_queue, (void*)trb);
        } else {
            list_append(xhci->event_queue, (void*)trb);
        }
    }

    // Update ERDP and clear event handler busy
    xhci->event_ring->regs->erdp = (uint64_t)(xhci->event_ring->trb_list_phys + (xhci->event_ring->dequeue * sizeof(xhci_trb_t))) | XHCI_ERDP_EHB;
}

/**
 * @brief xHCI IRQ handler
 * @param context The xHCI controller
 */
int xhci_irqHandler(void *context) {
    xhci_t *xhci = (xhci_t*)context;
    if (xhci) {
        // Acknowledge
        xhci_acknowledge(xhci, 0);

        LOG(DEBUG, "XHCI %p IRQ detected (event queue dq=%d, usbsts=%08x)\n", xhci, xhci->event_ring->dequeue, xhci->opregs->usbsts);

        // Poll event ring for port status change event IRQs
        // !!!: Because some real hardware doesn't support IRQs that well with xHCI (and we dont have MSI),
        // !!!: I use event queue polling sometimes.
        // !!!: If the events do get dequeued here, the waiters should have fallback code - but we need to pick a side.
        // !!!: (also should probably wake a thread up here rather than poll during the IRQ and stall the CPU)
        xhci_pollEventRing(xhci);
    }

    return 0;
}

/**
 * @brief Start xHCI controller
 * @param xhci The controller to start
 */
int xhci_startController(xhci_t *xhci) {
    // To start the controller, we need to set R/S, IE, and HOSTSYS_EE
    xhci->opregs->usbcmd |= XHCI_USBCMD_RUN_STOP;
    xhci->opregs->usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
    xhci->opregs->usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;

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
 * @brief Thread
 */
void xhci_thread(void *context) {
    xhci_t *xhci = (xhci_t*)context;
    for (;;) {
        if (xhci->port_queue->length) {
            node_t *n = list_popleft(xhci->port_queue);
            while (n) {
                xhci_port_status_change_trb_t *trb = (xhci_port_status_change_trb_t*)n->value;
                kfree(n);
                
                xhci_port_registers_t *port = (xhci_port_registers_t*)XHCI_PORT_REGISTERS(xhci->opregs, (trb->port_id-1));
                if (port->portsc & XHCI_PORTSC_CSC && port->portsc & XHCI_PORTSC_CCS) {
                    LOG(INFO, "Port %d connected\n", trb->port_id-1);
                    xhci_portInitialize(xhci, trb->port_id-1);
                } else {
                    LOG(INFO, "Port %d disconnected\n", trb->port_id-1);
                }

                n = list_popleft(xhci->port_queue);
            }
        }

        process_yield(1);
    }
}

/**
 * @brief Initialize an xHCI controller
 * @param device The PCI device of the xHCI controller
 */
int xhci_initController(pci_device_t *device) {
    LOG(INFO, "Initializing xHCI controller (bus %d, slot %d, function %d)\n", device->bus, device->slot, device->function);

    // Start by making a new xhci_t structure
    xhci_t *xhci = kzalloc(sizeof(xhci_t));
    xhci->pci = device;
    
    // Read in BAR0
    pci_bar_t *bar = pci_readBAR(device->bus, device->slot, device->function, 0);
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

    // Enable IRQs in PCI command
    uint16_t xhci_pci_command = pci_readConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, 2);
    xhci_pci_command &= ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_INTERRUPT_DISABLE); // Enable interrupts and disable I/O space
    xhci_pci_command |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE);
    pci_writeConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, xhci_pci_command, 2);

    // Try to enable MSI
    uint8_t irq = pci_enableMSI(device->bus, device->slot, device->function);
    if (irq != 0xFF) {
        if (hal_registerInterruptHandler(irq, xhci_irqHandler, (void*)xhci)) {
            LOG(ERR, "Error while registering IRQ%d\n", irq);
            return 1;
        }
    } else {
        LOG(WARN, "This xHCI controller does not support MSI - fallback to pin interrupt\n");
        irq = pci_getInterrupt(device->bus, device->slot, device->function);
        if (hal_registerInterruptHandler(irq, xhci_irqHandler, (void*)xhci)) {
            LOG(ERR, "Error while registering IRQ%d\n", irq);
            return 1;
        }
    }

    // Map it in
    LOG(DEBUG, "MMIO map: size 0x%016llX addr 0x%016llX bar type %d\n", bar->size, bar->address, bar->type);
    
    // !!!!: BUG BUG BUG IN THE PCI SYSTEM!!!!!!!!!!!!!
    if (!kargs_has("--xhci-fix-pci")) {
        xhci->mmio_addr = mem_mapMMIO(bar->address, bar->size);
    } else {
        xhci->mmio_addr = mem_mapMMIO((uint32_t)bar->address, (uint32_t)bar->size);
    }

    // Get some registers
    xhci->capregs = (xhci_cap_regs_t*)xhci->mmio_addr;
    xhci->opregs = (xhci_op_regs_t*)(xhci->mmio_addr + xhci->capregs->caplength);
    xhci->runtime = (xhci_runtime_regs_t*)(xhci->mmio_addr + xhci->capregs->rtsoff);
    LOG(DEBUG, "xHCI mapped MMIO to %p: capregs=%p opregs=%p runtime=%p\n", xhci->mmio_addr, xhci->capregs, xhci->opregs, xhci->runtime);

    // xhci_parseExtendedCapabilities(xhci);

    if (kargs_has("--xhci-fix-pci")) {
        xhci->bit64 = 0;
    } else {
        xhci->bit64 = XHCI_CSZ(xhci->capregs);
    }

    memcpy(&xhci->capregs_save, xhci->capregs, sizeof(xhci_cap_regs_t));

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

    // Create the event lists
    xhci->event_queue = list_create("xhci event queue");
    xhci->command_queue = list_create("xhci command queue");
    xhci->transfer_queue = list_create("xhci transfer queue");
    xhci->port_queue = list_create("xhci port queue");

    // Enable IRQs
    xhci->runtime->ir[0].iman |= XHCI_IMAN_INTERRUPT_ENABLE;

    // Enable the event ring
    if (xhci_initializeEventRing(xhci)) {
        LOG(ERR, "Error while initializing command ring\n");
        
        // TODO: Free scratcpad and cmd/event ring
        mem_freeDMA((uintptr_t)xhci->dcbaa, dcbaa_size);
        kfree(xhci->dcbaa_virt);
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        return 1;
    }

    // Clear pending
    xhci_acknowledge(xhci, 0);

    // Take over control (Panther Point)
    pci_writeConfigOffset(device->bus, device->slot, device->function, 0xD0, 0xFFFFFFFF, 4);
    pci_writeConfigOffset(device->bus, device->slot, device->function, 0xD8, 0xFFFFFFFF, 4);

    // Start the controller
    if (xhci_startController(xhci)) {
        LOG(ERR, "Error while starting controller\n");
        
        // TODO: Free scratcpad and cmd/event ring
        mem_freeDMA((uintptr_t)xhci->dcbaa, dcbaa_size);
        kfree(xhci->dcbaa_virt);
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        return 1;
    }

    // Create pool
    // TODO: Make pool size consistent with xHC
    xhci->ctx_pool = pool_create("xhci devctx pool", 64, PAGE_SIZE, 0, POOL_DMA);
    xhci->input_ctx_pool = pool_create("xhci inputctx pool", 64, PAGE_SIZE, 0, POOL_DMA);

    // Build the USB controller
    xhci->controller = usb_createController((void*)xhci, NULL);

    // Start probing
    LOG(INFO, "xHCI controller started successfully\n");
    for (uint8_t i = 0; i < 8; i++) {
        xhci_port_registers_t *port = (xhci_port_registers_t*)XHCI_PORT_REGISTERS(xhci->opregs, i);

        if (port->portsc & XHCI_PORTSC_CCS && port->portsc & XHCI_PORTSC_CSC) {
            LOG(DEBUG, "xHCI USB device detected on port %d\n", i);
            // xhci_portReset(xhci, i);
        }
    }

    // Spawn
    // TODO: This is temporary, and slow
    process_t *xhciproc = process_createKernel("xhci poller", PROCESS_KERNEL, PRIORITY_LOW, xhci_thread, (void*)xhci);
    scheduler_insertThread(xhciproc->main_thread);

    return 0;
}

/**
 * @brief Send a command TRB to a controller
 * @param xhci The controller to send the command TRB to
 * @param trb The TRB to send to the controller
 * @returns 0 on success
 */
xhci_command_completion_trb_t *xhci_sendCommand(xhci_t *xhci, xhci_trb_t *trb) {
    // !!!: LOCK!
    // LOG(DEBUG, "Command send: type=%d\n", trb->control.type);
    spinlock_acquire(&xhci->cmd_lock);

    // Enqueue the TRB
    if (xhci_enqueueTRB(xhci, trb)) {
        LOG(ERR, "Failed to enqueue TRB %p (type=0x%x)\n", trb, trb->control.type);
        spinlock_release(&xhci->cmd_lock);
        return NULL;
    }

    // Ring the doorbell    
    XHCI_RING_DOORBELL(xhci->capregs, 0, XHCI_DOORBELL_TARGET_COMMAND_RING);

    // Wait until we can pop from the queue
    // TODO: this sucks
    while (!xhci->command_queue->length) {
        LOG(DEBUG, "Waiting for xHCI %p to handle command IRQ: trb->type=%d usbsts=0x%08x\n", xhci, trb->control.type, xhci->opregs->usbsts);
        arch_pause();
    }

    // Pop the TRB from the queue
    node_t *node = list_popleft(xhci->command_queue);
    if (!node) {
        LOG(ERR, "Command IRQ handled but no TRB was found in the queue\n");
        spinlock_release(&xhci->cmd_lock);
        return NULL;
    }

    xhci_command_completion_trb_t *ctrb = (xhci_command_completion_trb_t*)node->value;
    kfree(node);

    LOG(INFO, "Command type=%d was completed with success code %d\n", trb->control.type, ctrb->completion_code);

    spinlock_release(&xhci->cmd_lock);
    return ctrb;
}