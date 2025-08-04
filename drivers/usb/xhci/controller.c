/**
 * @file drivers/usb/xhci/controller.c
 * @brief xHCI controller code
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
#include <kernel/misc/mutex.h>
#include <string.h>
#include <kernel/mem/pmm.h>

/* xHCI controller count */
int xhci_controller_count = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:CON ] " __VA_ARGS__);

/**
 * @brief Reset xHCI controller
 * @param xhci The controller to reset
 */
int xhci_resetController(xhci_t *xhci) {
    if (TIMEOUT(!(xhci->op->usbsts & XHCI_USBSTS_CNR), 500)) {
        LOG(ERR, "CNR in xHCI controller did not clear\n");
        return 1;
    }

    // Issue software reset
    xhci->op->usbcmd |= XHCI_USBCMD_HCRST;

    if (TIMEOUT(!(xhci->op->usbcmd & XHCI_USBCMD_HCRST), 500)) {
        LOG(ERR, "HCRST in xHCI controller did not clear\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Process extended capabilities
 * @param xhci Controller
 */
int xhci_processExtendedCapabilities(xhci_t *xhci) {
    uint16_t xECP = xhci->cap->extended_cap_pointer;
    if (!xECP) {
        LOG(WARN, "xECP not found\n");
        return 0; // TODO: Fatal?
    }

    uintptr_t ext = (uintptr_t)((uint8_t*)xhci->mmio_addr + xECP*4);   

    while (1) {
        volatile xhci_extended_capability_t *cap = (xhci_extended_capability_t*)ext;
        
        if (cap->id == XHCI_EXT_CAP_USBLEGSUP) {
            volatile xhci_legsup_capability_t *leg = (xhci_legsup_capability_t*)cap;

            leg->os_sem = 1;
            
            // Timeout until BIOS sem clears
            if (TIMEOUT(!(leg->bios_sem), 2000)) {
                LOG(ERR, "BIOS/OS handoff failure (BIOS did not release semaphore)\n");
                return 1;
            }

            LOG(DEBUG, "OS handoff success\n");
        } else if (cap->id == XHCI_EXT_CAP_SUPPORTED) {
            // Supported protocol list
            volatile xhci_supported_prot_capability_t *sup = (xhci_supported_prot_capability_t*)cap;

            if (sup->name_string != 0x20425355) {
                LOG(ERR, "ERROR: Supported capability ECP has invalid name string %08x\n", sup->name_string);
                return 1;
            }

            for (int i = sup->compat_port_offset; i < sup->compat_port_offset + sup->compat_port_count; i++) {
                // TODO: Apply this data to xhci->ports
                LOG(DEBUG, "Port %d has revision %d.%d\n", i, sup->major, sup->minor);
                xhci->ports[i-1].rev_major = sup->major;
                xhci->ports[i-1].rev_minor = sup->minor;
            }
        }

        if (!cap->next) break;
        ext += cap->next*4;
    }

    return 0;
}

/**
 * @brief Initialize xHCI command ring
 * @param xhci The xHCI to init the command ring on
 */
void xhci_initCommandRing(xhci_t *xhci) {
    xhci->cmd_ring = kzalloc(sizeof(xhci_cmd_ring_t));
    xhci->cmd_ring->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_COMMAND_RING_TRB_COUNT * sizeof(xhci_trb_t));
    xhci->cmd_ring->cycle = 1;
    memset(xhci->cmd_ring->trb_list, 0, XHCI_COMMAND_RING_TRB_COUNT * sizeof(xhci_trb_t));

    // Setup a link TRB to form a big chain
    xhci_link_trb_t *link = (xhci_link_trb_t*)(&xhci->cmd_ring->trb_list[XHCI_COMMAND_RING_TRB_COUNT-1]);
    link->ring_segment = mem_getPhysicalAddress(NULL, (uintptr_t)xhci->cmd_ring->trb_list);
    link->type = XHCI_TRB_TYPE_LINK;
    link->interrupter_target = 0;
    link->c = 1;
    link->tc = 1;
    link->ch = 0;
    link->ioc = 0;

    xhci->cmd_ring->enqueue = 0;

    // Set command ring in CRCR
    xhci->op->crcr = mem_getPhysicalAddress(NULL, (uintptr_t)xhci->cmd_ring->trb_list) | xhci->cmd_ring->cycle;
}

/**
 * @brief Initialize the xHCI event ring
 * @param xhci The controller
 */
void xhci_initEventRing(xhci_t *xhci) {
    xhci->event_ring = kzalloc(sizeof(xhci_event_ring_t));
    xhci->event_ring->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t) + sizeof(xhci_event_ring_entry_t));
    xhci->event_ring->ent = (xhci_event_ring_entry_t*)((uintptr_t)xhci->event_ring->trb_list + (XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t)));
    memset(xhci->event_ring->trb_list, 0, XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t) + sizeof(xhci_event_ring_entry_t));
    xhci->event_ring->cycle = 1;
    
    // Configure first entry in the list
    uintptr_t trb_list_phys = mem_getPhysicalAddress(NULL, (uintptr_t)xhci->event_ring->trb_list);
    xhci->event_ring->ent->rsba = trb_list_phys;
    xhci->event_ring->ent->rsz = XHCI_EVENT_RING_TRB_COUNT;
    xhci->event_ring->ent->rsvd0 = 0;

    // Setup registers
    xhci->run->irs[0].erstsz = 1;
    xhci->run->irs[0].erdp = trb_list_phys | (1 << 3);
    xhci->run->irs[0].erstba = trb_list_phys + (XHCI_EVENT_RING_TRB_COUNT * sizeof(xhci_trb_t));

    xhci->event_ring->trb_list_phys = trb_list_phys;
    LOG(DEBUG, "Event ring enabled (TRB list: %016llX)\n", trb_list_phys);
}

/**
 * @brief IRQ handler
 * @param context xHCI
 */
int xhci_irq(void *context) {
    xhci_t *xhci = (xhci_t*)context;

    // Acknowledge
    XHCI_ACKNOWLEDGE(xhci);

    // Clear EINT
    if (!(xhci->op->usbsts & XHCI_USBSTS_EINT)) return 0;
    xhci->op->usbsts = XHCI_USBSTS_EINT;

    while (1) {
        xhci_trb_t *t = xhci_dequeueEventTRB(xhci);
        if (!t) break;
        
        if (t->type == XHCI_EVENT_PORT_STATUS_CHANGE) {
            xhci_port_status_change_trb_t *trb = (xhci_port_status_change_trb_t*)t;
            LOG(INFO, "Port status change event detected on port %d\n", trb->port_id);

            // Wakeup poller thread
            if (xhci->poller) sleep_wakeup(xhci->poller->main_thread);
        } else if (t->type == XHCI_EVENT_COMMAND_COMPLETION) {
            xhci_command_completion_trb_t *trb = (xhci_command_completion_trb_t*)t;
            LOG(INFO, "Command completion event detected\n");
            xhci->ctr = trb;
            __atomic_store_n(&xhci->flag, 1, __ATOMIC_SEQ_CST);
        } else if (t->type == XHCI_EVENT_TRANSFER) {
            xhci_transfer_completion_trb_t *trb = (xhci_transfer_completion_trb_t*)t;
            LOG(INFO, "Transfer completed on slot %d endp %d cc %d\n", trb->slot_id, trb->endpoint_id, trb->completion_code);
            
            xhci->slots[trb->slot_id-1]->endpoints[trb->endpoint_id-1].ctr = trb;
            __atomic_store_n(&xhci->slots[trb->slot_id-1]->endpoints[trb->endpoint_id-1].flag, 1, __ATOMIC_SEQ_CST);
        } else {
            LOG(WARN, "Unrecognized event TRB: %d\n", t->type);
        }
    }


    // Reprogram ERDP without EHB
    xhci->run->irs[0].erdp = (uint64_t)(xhci->event_ring->trb_list_phys + (xhci->event_ring->dequeue * sizeof(xhci_trb_t))) | XHCI_ERDP_EHB;

    return 0;
}

/**
 * @brief Initialize interrupts
 * @param xhci The controller
 */
int xhci_initInterrupt(xhci_t *xhci) {
    uint8_t irq = pci_enableMSI(xhci->dev->bus, xhci->dev->slot, xhci->dev->function);
    if (irq == 0xFF) {
        LOG(INFO, "Using PCI pin interrupts\n");
        irq = pci_getInterrupt(xhci->dev->bus, xhci->dev->slot, xhci->dev->function);

        if (irq == 0xFF) {
            LOG(ERR, "xHCI could not find a valid interrupt\n");
            return 1;
        }
    } else {
        LOG(INFO, "Using MSI interrupt\n");
    }

    hal_registerInterruptHandler(irq, xhci_irq, (void*)xhci);
    LOG(DEBUG, "IRQ%d in use for xHCI controller\n", irq);

    // Enable IRQs in the PCI device
    return 0;
}

/**
 * @brief Initialize scratchpad registers
 * @param xhci The controller
 */
void xhci_initScratchpad(xhci_t *xhci) {
    uint32_t scratchpads = (xhci->cap->max_scratchpad_buffers_hi << 5) | (xhci->cap->max_scratchpad_buffers_lo);
    xhci->scratchpad = mem_allocateDMA(scratchpads * sizeof(uint64_t));
    uintptr_t phys = mem_getPhysicalAddress(NULL, xhci->scratchpad);

    // Fill scratchpad buffers
    for (size_t i = 0; i < scratchpads; i++) {
        uintptr_t pg = pmm_allocateBlock();
        ((uint64_t*)xhci->scratchpad)[i] = pg;
    }

    *(uint64_t*)(xhci->dcbaa) = mem_getPhysicalAddress(NULL, xhci->scratchpad);
}

/**
 * @brief xHCI port thread
 */
void xhci_thread(void *context) {
    xhci_t *xhci = (xhci_t*)context;

    // Bug (QEMU): We run an initial pass on xHCI devices since (while the controller is *supposed* to send us Port Status Change TRBs, it doesn't.)
    for (;;) {
        for (unsigned i = 0; i < xhci->cap->max_ports; i++) {
            if (xhci->ports[i].rev_major) {
                // We have a port
                volatile xhci_port_regs_t *r = (xhci_port_regs_t*)&xhci->op->ports[i];
                
                // Check flags
                uint32_t portsc_saved = r->portsc;
                r->portsc = XHCI_PORTSC_CSC | XHCI_PORTSC_PRC | XHCI_PORTSC_PP;

                // If we are not connected, then.
                if (!(r->portsc & XHCI_PORTSC_CCS)) {
                    continue;
                }

                // Check revision
                if (xhci->ports[i].rev_major == 2) {
                    // USB2 port requires reset first
                    if (!(portsc_saved & XHCI_PORTSC_PED && portsc_saved & XHCI_PORTSC_PRC)) {
                        // Port needs a reset
                        r->portsc = XHCI_PORTSC_PR | XHCI_PORTSC_PP;
                        continue;
                    }
                } else if (xhci->ports[i].rev_major == 3) {
                    // USB3 ports are fine
                    if (!(portsc_saved & XHCI_PORTSC_CSC && portsc_saved & XHCI_PORTSC_PED)) {
                        continue;
                    }
                }

                LOG(INFO, "Device detected on port %d\n", i);
                xhci_initializeDevice(xhci, i);
            }
        }

        sleep_untilNever(current_cpu->current_thread);
        sleep_enter();
    }
}

/**
 * @brief Dequeue event TRB
 * @param xhci The xHCI controller
 * @returns TRB on success or NULL
 */
xhci_trb_t *xhci_dequeueEventTRB(xhci_t *xhci) {
    if (XHCI_EVENT_RING_DEQUEUE(xhci)->c != xhci->event_ring->cycle) return NULL; // No content available
    xhci_trb_t *trb = XHCI_EVENT_RING_DEQUEUE(xhci);
    
    // Wrap dequeue
    xhci->event_ring->dequeue++;
    if (xhci->event_ring->dequeue >= XHCI_EVENT_RING_TRB_COUNT) {
        xhci->event_ring->dequeue = 0;
        xhci->event_ring->cycle ^= 1;
    }

    return trb;
}

/**
 * @brief Enqueue command TRB
 * @param xhci The xHCI controller
 * @param trb The TRB to enqueue
 * @returns 0 on success
 */
int xhci_enqueueCommandTRB(xhci_t *xhci, xhci_trb_t *trb) {
    // Place our current TRB
    trb->c = xhci->cmd_ring->cycle;
    memcpy(&xhci->cmd_ring->trb_list[xhci->cmd_ring->enqueue], trb, sizeof(xhci_trb_t));

    // Next enqueue
    xhci->cmd_ring->enqueue++;
    if (xhci->cmd_ring->enqueue >= XHCI_COMMAND_RING_TRB_COUNT-1) {
        // Update link TRB
        xhci_link_trb_t *link = (xhci_link_trb_t*)(&xhci->cmd_ring->trb_list[XHCI_COMMAND_RING_TRB_COUNT-1]);
        link->type = XHCI_TRB_TYPE_LINK;
        link->tc = 1;
        link->c = xhci->cmd_ring->cycle;
        
        xhci->cmd_ring->enqueue = 0;
        xhci->cmd_ring->cycle ^= 1;
    }

    return 0;
}

/**
 * @brief Initialize xHCI controller
 * @param device The PCI device
 */
int xhci_initController(pci_device_t *device) {
    LOG(INFO, "xHCI initializing controller: bus %d, slot %d, func %d\n", device->bus, device->slot, device->function);

    // First, enable bus mastering + MMIO, disable IRQs
    uint16_t cmd = pci_readConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, 2);
    pci_writeConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, ((cmd & ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_INTERRUPT_DISABLE))) | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_INTERRUPT_DISABLE, 2);

    // Make xHCI structure
    xhci_t *xhci = kzalloc(sizeof(xhci_t));
    xhci->dev = device;

    // Read in BAR0
    pci_bar_t *bar = pci_readBAR(device->bus, device->slot, device->function, 0);
    if (!bar) {
        LOG(ERR, "xHCI does not have BAR0\n");
        kfree(xhci);
        return 1;
    }
    
    // Check BAR type
    if (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64) {
        // BAR is not valid type
        LOG(ERR, "xHCI BAR0 is of unrecognized type %d\n", bar->type);
        kfree(xhci);
        kfree(bar);
        return 1;
    }

    LOG(DEBUG, "xHCI MMIO is located at %016llX - %016llX\n", bar->address, bar->address + bar->size);

    // Map BAR region
    xhci->mmio_addr = mem_mapMMIO(bar->address, bar->size);
    xhci->cap = (xhci_cap_regs_t*)xhci->mmio_addr;
    xhci->op = (xhci_op_regs_t*)(xhci->mmio_addr + xhci->cap->caplength);
    xhci->run = (xhci_runtime_regs_t*)(xhci->mmio_addr + xhci->cap->rtsoff);
    xhci->mutex = mutex_create("xhci mutex");
    
    // Reset xHCI controller
    if (xhci_resetController(xhci)) {
        LOG(ERR, "xHCI controller reset failed\n");
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        kfree(bar);
        return 1;
    }

    LOG(INFO, "xHCI controller: version %d.%d.%d\n", +xhci->cap->revision_major, xhci->cap->revision_minor >> 4, xhci->cap->revision_minor & 0xF);
    LOG(INFO, "\tMaximum ports: %d Maximum slots: %d Maximum interrupters: %d\n", xhci->cap->max_slots, xhci->cap->max_slots, xhci->cap->max_interrupters);

    // Allocate ports and slots array
    xhci->ports = kzalloc(sizeof(xhci_port_info_t) * xhci->cap->max_ports);
    xhci->slots = kzalloc(sizeof(xhci_device_t*) * xhci->cap->max_slots);

    // Process xECP
    if (xhci_processExtendedCapabilities(xhci)) {
        LOG(ERR, "xHCI ECP parse error failed\n");
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        kfree(bar);
        return 1;
    }

    // Now, allocate the DCBAA
    xhci->dcbaa = mem_allocateDMA(xhci->cap->max_slots * 8);
    memset((void*)xhci->dcbaa, 0, xhci->cap->max_slots * 8);
    xhci->op->dcbaap = mem_getPhysicalAddress(NULL, xhci->dcbaa);
    LOG(DEBUG, "DCBAA @ %p\n", xhci->op->dcbaap);

    // Init command ring
    xhci_initCommandRing(xhci);

    // Init interrupt
    if (xhci_initInterrupt(xhci)) {
        // TODO: Free all memory
        LOG(ERR, "xHCI interrupter init failed\n");
        mem_unmapMMIO(xhci->mmio_addr, bar->size);
        kfree(xhci);
        kfree(bar);
        return 1;
    }

    // Initialize event ring
    xhci_initEventRing(xhci);

    // Initialize scratchpad
    xhci_initScratchpad(xhci);
    
    // Enable interrupters
    XHCI_ACKNOWLEDGE(xhci);
    xhci->op->usbcmd |= XHCI_USBCMD_INTE;

    // Run controller
    xhci->op->usbcmd |= XHCI_USBCMD_RS;
    while (xhci->op->usbsts & XHCI_USBSTS_HCH) arch_pause_single();
    LOG(INFO, "xHCI controller started\n");

    // Prepare controller
    xhci->controller = usb_createController((void*)xhci, NULL);

    // Spawn poller thread
    xhci->poller = process_createKernel("xhci poller", PROCESS_KERNEL, PRIORITY_LOW, xhci_thread, (void*)xhci);
    scheduler_insertThread(xhci->poller->main_thread);

    // Controller initialized successfully
    xhci_controller_count++;
    return 0;
}

/**
 * @brief Send a command to the xHCI controller
 * @param xhci The xHCI controller
 * @param trb The command TRB
 * @returns TRB on success, NULL is an error
 */
xhci_command_completion_trb_t* xhci_sendCommand(xhci_t *xhci, xhci_trb_t *trb) {
    mutex_acquire(xhci->mutex);
    xhci->ctr = NULL;

    __atomic_store_n(&xhci->flag, 0, __ATOMIC_SEQ_CST);

    // Enqueue, please.
    xhci_enqueueCommandTRB(xhci, trb);

    // Ring ring
    XHCI_DOORBELL(xhci, 0) = 0;
    
    // TODO: This was too slow..
    // if (TIMEOUT(__atomic_load_n(&xhci->flag, __ATOMIC_SEQ_CST) == 1, 2500)) {
    //     LOG(ERR, "Timed out while waiting for xHC to handle command of type %d\n", trb->type);
    //     mutex_release(xhci->mutex);
    //     return NULL;
    // }

    while (__atomic_load_n(&xhci->flag, __ATOMIC_SEQ_CST) != 1) {
        if (current_cpu->current_thread) {
            process_yield(1);
        } else {
            arch_pause_single();
        }
    }

    xhci_command_completion_trb_t *ctrb = xhci->ctr;
    LOG(DEBUG, "TRB complete\n");

    if (!TRB_SUCCESS(ctrb)) {
        LOG(ERR, "TRB failed with completion code: %d\n", ctrb->cc);
        mutex_release(xhci->mutex);
        return NULL;
    }

    mutex_release(xhci->mutex);
    return ctrb;
}