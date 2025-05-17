/**
 * @file drivers/uhci/uhci.c
 * @brief Universal Host Controller Interface driver
 * 
 * @warning Does not work on x86_64 for some reason
 * 
 * @todo Bulk transfers, interrupt transfers, isochronous transfers (not sure if we can do the last now)
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "uhci.h"

#include <kernel/loader/driver.h>

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/dev.h>
#include <kernel/drivers/clock.h>
#include <kernel/drivers/pci.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:UHCI", __VA_ARGS__)

/**
 * @brief Create a queue head
 * @param hc The host controller
 */
static uhci_qh_t *uhci_createQH(uhci_t *hc) {
    if (!hc) return NULL;

    uhci_qh_t *qh = (uhci_qh_t*)pool_allocateChunk(hc->qh_pool);
    if (!qh) {
        // Temporary
        kernel_panic_extended(OUT_OF_MEMORY, "uhci-qhpool", "*** No more memory remaining to allocate queue heads (KERNEL BUG)\n");
    }

    memset(qh, 0, sizeof(uhci_qh_t));
    qh->td_list = list_create("td list");   // !!!: This probably shouldn't be here

    // LOG(DEBUG, "[QH] New QH created at %p/%p\n", qh, mem_getPhysicalAddress(NULL, (uintptr_t)qh));

    return qh;
}

/**
 * @brief Allocate and create a new transfer descriptor
 * 
 * @param hc The host controller
 * @param speed The speed of the transfer descriptor
 * @param toggle Syncronization - see https://wiki.osdev.org/Universal_Serial_Bus#Data_Toggle_Synchronization
 * @param devaddr The device address
 * @param endp The endpoint of the transfer
 * @param type The type of the packet, for example @c UHCI_PACKET_IN
 * @param length The length of the transfer, not including protocol bytes like PID/CRC
 * @param data The data of the transfer (usually a descriptor)
 *
 * @returns A new TD - make sure to link it to the prev one
 */
uhci_td_t *uhci_createTD(uhci_t *hc, int speed, uint32_t toggle, uint32_t devaddr, uint32_t endp, uint32_t type, uint32_t length, void *data) {
    // Allocate a transfer descriptor
    uhci_td_t *td = (uhci_td_t*)pool_allocateChunk(hc->td_pool);
    if (!td) {
        // Temporary
        kernel_panic_extended(OUT_OF_MEMORY, "uhci-tdpool", "*** No more memory remaining to allocate transfer descriptors (KERNEL BUG)\n");
    }
    memset(td, 0, sizeof(uhci_td_t));

    if (speed == USB_HIGH_SPEED) {
        LOG(WARN, "USB_HIGH_SPEED is not supported by the UHCI controller. Assuming full speed\n");
    }

    // Setup TD speed and set it active
    td->cs.ls = (speed == USB_LOW_SPEED) ? 1 : 0;
    td->cs.active = 1;
    
    // Setup the errors field to have 3 errors
    // The TD will be marked as stalled if this hits 0
    td->cs.errors = 3;
    
    // Setup the link pointer (by default terminate)
    TD_LINK_TERM(td);

    // Create the length field and set it in token
    length = (length == 0) ? 0x7FF : (length - 1) & 0x7FF;
    td->token.maxlen = length;
    
    // Setup the rest of token values
    // td->token.endpt = endp;
    td->token.raw |= endp << 15;
    td->token.pid = type;
    td->token.device_addr = devaddr;
    td->token.d = toggle;

    // Setup buffer
    td->buffer = (uint32_t)(uintptr_t)mem_getPhysicalAddress(NULL, (uintptr_t)data);

    // LOG(DEBUG, "[TD] New TD created at %p/%p - type 0x%x ls %i devaddr 0x%x toggle 0x%x endp 0x%x\n", td, mem_getPhysicalAddress(NULL, (uintptr_t)td), td->token.pid, td->cs.ls, td->token.device_addr, td->token.d, td->token.endpt);
    // LOG(DEBUG, "[TD] Pool system has %d bytes remaining\n", hc->td_pool->allocated - hc->td_pool->used);

    // Done
    return td;
}

/**
 * @brief Destroy a queue head, freeing all of its TDs, removing it from the chain, and freeing it
 * 
 * @param controller The controller
 * @param qh The queue head
 */
void uhci_destroyQH(USBController_t *controller, uhci_qh_t *qh) {
    uhci_t *hc = HC(controller);

    // First we need to unlink the queue head from the chain
    spinlock_acquire(&hc->lock);
    node_t *node = list_find(hc->qh_list, (void*)qh);
    if (node) {
        uhci_qh_t *qh_prev = (uhci_qh_t*)node->prev->value;
        if (hc->qh_list->tail == node) {
            // This is the end of the list
            QH_LINK_TERM(qh_prev);
            qh_prev->qh_link.qhlp = 0x0;
        } else {
            // There are still more queue heads to process
            uhci_qh_t *qh_next = (uhci_qh_t*)node->next->value;
            QH_LINK_QH(qh_prev, qh_next);
        }

        // Delete from the list
        list_delete(hc->qh_list, node);
    } else {
        LOG(WARN, "Tried to destroy queue head that is not apart of HC list\n");
    }

    // Zero the queue element pointer
    qh->qe_link.terminate = 1;
    qh->qe_link.qelp = 0;

    // Each TD is allocated to a pool rather than by kmalloc
    foreach(td_node, qh->td_list) {
        uhci_td_t *td = (uhci_td_t*)td_node->value;
        // LOG(DEBUG, "[TD] TD at %p destroyed\n", td);
        pool_freeChunk(hc->td_pool, (uintptr_t)td);
    }
    
    list_destroy(qh->td_list, false);
    
    // Free the queue head
    pool_freeChunk(hc->qh_pool, (uintptr_t)qh);
    // LOG(DEBUG, "[QH] QH at %p destroyed\n", qh);

    spinlock_release(&hc->lock);
}


/**
 * @brief Write to a port (PORTSC1 or PORTSC2)
 * @param port The port to write to (should be PORTSC1 or PORTSC2)
 * @param value The value to write
 * @todo Make this a macro
 */
void uhci_writePort(uint32_t port, uint16_t data) {
    // First we have to read the port to make sure that we're not overwriting anything
    uint16_t current = inportw(port);
    current |= data;
    current &= ~UHCI_PORT_RWC; // Clear the change flags
    current &= ~((1 << 5) | (1 << 4) | (1 << 0)); // Clear reserved
    outportw(port, current);
}

/**
 * @brief Clears a port's flags
 * @param port The port to clear (should be PORTSC1 or PORTSC2)
 * @param flag The flag(s) to clear
 * @todo Make this a macro
 */
void uhci_clearPort(uint32_t port, uint16_t data)  {
    uint16_t current = inportw(port);
    current &= ~UHCI_PORT_RWC;          // Clear the change flags
    current &= ~data;
    current |= UHCI_PORT_RWC & data;    // Set the change flag if data changed one of these bits
    current &= ~((1 << 5) | (1 << 4) | (1 << 0)); // Clear reserved
    outportw(port, current);
}

/**
 * @brief Probe for UHCI devices
 * @param controller The USB controller to probe devices from
 * 
 * @returns The number of found devices
 */
int uhci_probe(USBController_t *controller) {
    uhci_t *hc = HC(controller);
    if (!hc) return 0;

    int found_ports = 0;

    // UHCI controllers have a max of 2 ports directly connected
    for (uint32_t port = 0; port < 2; port++) {
        // Get the PORTSC address
        uint32_t port_addr = UHCI_REG_PORTSC1 + (port*2);

        // We can probe for devices by resetting the port and checking if CONNECTION_CHANGE was set
        LOG(DEBUG, "UHCI resetting port 0x%x\n", hc->io_addr + port);
        uhci_writePort(hc->io_addr + port_addr, UHCI_PORT_RESET);
        clock_sleep(100);   // Wait 100ms for the port to reset
        uhci_clearPort(hc->io_addr + port_addr, UHCI_PORT_RESET);

        int port_enabled = 0;

        // Now we can wait ~200ms (it's required to wait at least 100ms) while checking the status
        uint16_t status; // This will be used to determine the speed of the port
        for (int i = 0; i < 20; i++) {
            clock_sleep(10); // Sleep 10ms

            // Read the status and check if a connection change has occurred
            status = inportw(hc->io_addr + port_addr);
            if (!(status & UHCI_PORT_CONNECTION)) {
                break;
            }

            // Acknowledge an RWC if necessary
            if (status & UHCI_PORT_RWC) {
                uhci_clearPort(hc->io_addr + port_addr, UHCI_PORT_RWC);
                continue;
            }

            // Has the port completed its enabling process?
            if (status & UHCI_PORT_ENABLE) {
                // Port enabled
                port_enabled = 1;
                break;
            }

            // Nope, enable the port
            uhci_writePort(hc->io_addr + port_addr, UHCI_PORT_ENABLE);
        }

        if (port_enabled) {
            // The port was successfully enabled
            found_ports++;
            LOG(DEBUG, "Found a UHCI device connected to port %i\n", port);

            // Now, we need to initialize the device connected to the port
            USBDevice_t *dev = usb_createDevice(controller, port, (status & UHCI_PORT_LSDA) ? USB_LOW_SPEED : USB_FULL_SPEED, NULL, uhci_control, uhci_interrupt);
            dev->mps = (dev->speed == USB_LOW_SPEED) ? 8 : 64;
 
            if (usb_initializeDevice(dev)) {
                LOG(ERR, "Failed to initialize UHCI device\n");
                
                usb_destroyDevice(controller, dev);
                found_ports--;
                break;
            }
        }
    }

    LOG(INFO, "Successfully initialized %i devices\n", found_ports);
    return found_ports;
}

/**
 * @brief Wait for a queue head to complete its transfer
 * @param controller The controller
 * @param qh The queue head to watch
 * 
 * @returns Nothing, but sets transfer status to USB_TRANSFER_SUCCESS or USB_TRANSFER_FAILED
 */
void uhci_waitForQH(USBController_t *controller, uhci_qh_t *qh) {
    if (!controller || !qh || !controller->hc) return;
    USBTransfer_t *transfer = qh->transfer;

    

    // The UHCI controller will update the queue head's link pointer to be the link pointer of the next TD it is processing
    // Therefore when it reaches the end it should point to 0x0, as that is the terminating TD's link pointer.
    if (qh->qe_link.qelp == 0x0) {
        // Finished!
        transfer->status = USB_TRANSFER_SUCCESS;
    } else {
        // TODO: There has to be a better way to do this (specifically in Hexahedron terms), as mem_remapPhys sucks in i386.
        uhci_td_t *td_remapped = (uhci_td_t*)mem_remapPhys((uintptr_t)(qh->qe_link.qelp << 4), sizeof(uhci_td_t));
        
        if (!(td_remapped->cs.active)) {
            // The TD was marked as not active?
            if (td_remapped->cs.stalled) {
                // Stalled :(
                LOG(ERR, "UHCI controller detected a fatal TD stall - transfer terminated\n");
                LOG(ERR, "Transfer terminated - controller could not process physical TD %p (PID 0x%x)\n", qh->qe_link.qelp << 4, td_remapped->token.pid);
                transfer->status = USB_TRANSFER_FAILED;
            }

            // TODO: Add more errors to print out
        }

        mem_unmapPhys((uintptr_t)td_remapped, sizeof(uhci_td_t));
    }
} 



/**
 * @brief UHCI control transfer method
 */
int uhci_control(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer) {
    if (!controller || !dev || !transfer || !controller->hc) return USB_TRANSFER_FAILED;
    uhci_t *hc = HC(controller);

    // A CONTROL transfer consists of 3 parts:
    // 1. A SETUP packet that details the transaction
    // 2. Some DATA packets that convey transfer->data in terms of dev->mps
    // 3. A STATUS packet to complete the transaction

    // First, create the queue head that will hold the packets/TDs
    uhci_qh_t *qh = uhci_createQH(hc);
    qh->transfer = transfer;
    QH_LINK_TERM(qh);


    // Setup variables that will change on the TDs
    uint32_t toggle = 0; // Toggle bit

    // Create the SETUP transfer descriptor
    uhci_td_t *td_setup = uhci_createTD(hc, dev->speed, toggle, dev->address, transfer->endpoint, UHCI_PACKET_SETUP, 8, (void*)(uintptr_t)transfer->req);
    QH_LINK_TD(qh, td_setup);

    // Now create the DATA descriptors. These need to be limited to dev->mps but do not need to be padded.
    uint8_t *buffer = (uint8_t*)transfer->data;
    uint8_t *buffer_end = (uint8_t*)((uintptr_t)transfer->data + (uintptr_t)transfer->length);
    uhci_td_t *last = td_setup;
    while (buffer < buffer_end) {
        uint32_t transaction_size = buffer_end - buffer;
        if (transaction_size > dev->mps) transaction_size = dev->mps; // Limit

        if (!transaction_size) break;
        // Now create the TD 
        toggle ^= 1;
        uhci_td_t *td = uhci_createTD(hc, dev->speed, toggle, dev->address, transfer->endpoint, 
                                        (transfer->req->bmRequestType & USB_RT_D2H) ? UHCI_PACKET_IN : UHCI_PACKET_OUT, 
                                        transaction_size, (void*)(uintptr_t)buffer);
        
        TD_LINK_TD(qh, last, td);


        // Update variables and go again
        buffer += transaction_size;
        last = td;
    }

    // Now all we have to do is create a STATUS packet to complete the chain
    uhci_td_t *td_status = uhci_createTD(hc, dev->speed, 1, dev->address, transfer->endpoint,
                                (transfer->req->bmRequestType & USB_RT_D2H) ? UHCI_PACKET_OUT : UHCI_PACKET_IN,
                                0, NULL);

    TD_LINK_TD(qh, last, td_status);
    TD_LINK_TERM(td_status);

    // Insert it into the chain
    spinlock_acquire(&hc->lock);
    uhci_qh_t *current = (uhci_qh_t*)hc->qh_list->tail->value;
    QH_LINK_QH(current, qh);
    list_append(hc->qh_list, (void*)qh);
    spinlock_release(&hc->lock);

    // Wait for the transfer to finish
    while (transfer->status == USB_TRANSFER_IN_PROGRESS) {
        uhci_waitForQH(controller, qh);
    }

    // Destroy the queue head
    uhci_destroyQH(controller, qh); 

    return transfer->status;
}

/**
 * @brief UHCI interrupt transfer method
 */
int uhci_interrupt(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer) {
    if (!controller || !dev || !transfer || !HC(controller)) return USB_TRANSFER_FAILED;
    uhci_t *hc = HC(controller);

    // An INTERRUPT transfer consists of a single input TD
    
    // First, create the queue head that will hold the TD
    uhci_qh_t *qh = uhci_createQH(hc);
    qh->transfer = transfer;
    QH_LINK_TERM(qh);

    // Create transfer descriptor
    uhci_td_t *td = uhci_createTD(hc, dev->speed, transfer->endp->toggle, dev->address, transfer->endpoint, UHCI_PACKET_IN, transfer->length, transfer->data);
    TD_LINK_TERM(td);
    QH_LINK_TD(qh, td);

    // Insert it into the chain
    spinlock_acquire(&hc->lock);
    uhci_qh_t *current = (uhci_qh_t*)hc->qh_list->tail->value;
    QH_LINK_QH(current, qh);
    list_append(hc->qh_list, (void*)qh);
    spinlock_release(&hc->lock);

    // Wait for the transfer to finish
    while (transfer->status == USB_TRANSFER_IN_PROGRESS) {
        uhci_waitForQH(controller, qh);
        arch_pause(); // !!! TEMPORARY
    }

    // Destroy the queue head
    uhci_destroyQH(controller, qh); 

    if (transfer->endp) {
        transfer->endp->toggle ^= 1;
    }

    return transfer->status;
}

/**
 * @brief UHCI initialize controller
 */
void uhci_initController(uint32_t uhci_pci) {
    LOG(DEBUG, "Initializing UHCI controller on bus %d slot %d function %d\n", PCI_BUS(uhci_pci), PCI_SLOT(uhci_pci), PCI_FUNCTION(uhci_pci));

    // Now read in the PCI bar
    pci_bar_t *bar = pci_readBAR(PCI_BUS(uhci_pci), PCI_SLOT(uhci_pci), PCI_FUNCTION(uhci_pci), 4);
    if (!bar) {
        LOG(ERR, "UHCI controller does not have BAR4 - false positive?\n");
        return;
    }

    if (!(bar->type == PCI_BAR_IO_SPACE)) {
        LOG(ERR, "UHCI controller BAR4 is not I/O space - bug in PCI driver?\n");
        return;
    }

    // Construct a host controller
    uhci_t *hc = kmalloc(sizeof(uhci_t));
    memset(hc, 0, sizeof(uhci_t));
    hc->io_addr = bar->address;

    // Allocate a frame list (4KB alignment)
    // !!!: We don't need a DMA region but we need a region that is below 0xFFFFFFFF
    hc->frame_list = (uhci_flp_t*)mem_allocateDMA(4096);
    memset(hc->frame_list, 0, 4096);

    LOG(DEBUG, "Frame list allocated to %p\n", hc->frame_list);
    
    // Do a check
    if ((sizeof(uhci_qh_t) % 16) || (sizeof(uhci_td_t) % 16))  {
        LOG(ERR, "You are missing the 16-byte alignment required for TDs/QHs\n");
        LOG(ERR, "Please modify the uhci.h header file to add an extra DWORD as your architecture is 64-bit\n");
        LOG(ERR, "Require a 16-byte alignment but QH = %d and TD = %d\n", sizeof(uhci_qh_t), sizeof(uhci_td_t));
        return;
    } 

    
    // Create the pools (16-byte alignment)
    hc->qh_pool = pool_create("uhci qh pool", sizeof(uhci_qh_t), 512 * sizeof(uhci_qh_t), 0, POOL_DMA);
    hc->td_pool = pool_create("uhci td pool", sizeof(uhci_td_t), 512 * sizeof(uhci_td_t), 0, POOL_DMA);

    // Create the queue head list (and the first terminating queue head)
    hc->qh_list = list_create("uhci qh list");
    uhci_qh_t *qh = uhci_createQH(hc);
    list_append(hc->qh_list, (void*)qh);

    QH_LINK_TERM(qh);
    qh->qe_link.terminate = 1; // Terminate the QE list

    // This queue head that was just created will serve as the first queue head of all transactions
    // When a new transaction is created, it will be linked to that transaction's queue head and have its terminate bit disabled.
    // When a transaction is completed (and that transaction was the last one), it will have its terminate bit reset.

    // Make frame list skeleton
    for (int i = 0; i < 1024; i++) {
        hc->frame_list[i].qh = 1;
        hc->frame_list[i].flp = LINK(qh);
        hc->frame_list[i].terminate = 0;
    }

    // Mark entry 1024 as terminate
    hc->frame_list[1023].terminate = 1;

    // Configure the UHCI controller
    outportw(hc->io_addr + UHCI_REG_LEGSUP, 0x8F00);        // Disable legacy support
    outportw(hc->io_addr + UHCI_REG_USBINTR, 0x0);          // Disable interrupts
    outportw(hc->io_addr + UHCI_REG_FRNUM, 0);              // Assign framelist 

    // Frame list is expected as a physical address
    uintptr_t frame_list_physical = mem_getPhysicalAddress(NULL, (uintptr_t)hc->frame_list);
    outportl(hc->io_addr + UHCI_REG_FLBASEADD, (uint32_t)frame_list_physical & ~0xFFF);
    outportw(hc->io_addr + UHCI_REG_SOFMOD, 0x40);          // Use a default of 64, which gives a SOF cycle time of 12000
    outportw(hc->io_addr + UHCI_REG_USBSTS, 0x00);          // Reset status
    outportw(hc->io_addr + UHCI_REG_USBCMD, UHCI_CMD_RS);   // Enable controller

    // Create controller
    USBController_t *controller = usb_createController((void*)hc, NULL); // TODO: No polling method as no asyncronous transfers/port insertion detection yet

    // Probe for devices
    // TODO: For the USB stack, make the main USB logic probe for devices
    uhci_probe(controller);

    // Register the controller
    usb_registerController(controller);
}

/**
 * @brief UHCI controller find method
 */
int uhci_find(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    // We know this device is of type 0x0C03, but it's only UHCI if the interface is 0x00
    if (pci_readConfigOffset(bus, slot, function, PCI_PROGIF_OFFSET, 1) == 0x00) {
        uhci_initController(PCI_ADDR(bus, slot, function, 0));
    }

    return 0;
}



/**
 * @brief UHCI initialize method
 */
int uhci_init(int argc, char **argv) {
    // Scan and find the UHCI PCI device
    pci_scan(uhci_find, NULL, 0x0C03);
    return 0;
}

/**
 * @brief UHCI deinitialize method
 */
int uhci_deinit() {
    return 0;
}


/* Metadata */
struct driver_metadata driver_metadata = {
    .name = "UHCI Driver",
    .author = "Samuel Stuart",
    .init = uhci_init,
    .deinit = uhci_deinit
};