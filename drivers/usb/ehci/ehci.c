/**
 * @file drivers/usb/ehci/ehci.c
 * @brief Enhanced Host Controller Interface driver
 * 
 * @note This driver is shoddy and needs some work.
 * @todo Support for VBox and Bochs
 * @todo Refine USBLEGSUP code
 * @todo Put CONTROL transfers in asyncronous
 *
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "ehci.h"

#include <kernel/loader/driver.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/clock.h>
#include <kernel/drivers/pci.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <structs/list.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <string.h>

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#else
#include <kernel/arch/x86_64/hal.h>
#endif

/* Macros to read/write values to port memory */
#define EHCI_OP_WRITE8(reg, value) (*((uint8_t*)(hc->op_base + reg)) = (uint8_t)value)
#define EHCI_OP_WRITE16(reg, value) (*((uint16_t*)(hc->op_base + reg)) = (uint16_t)value)
#define EHCI_OP_WRITE32(reg, value) (*((uint32_t*)(hc->op_base + reg)) = (uint32_t)value)
#define EHCI_OP_READ8(reg) (*(uint8_t*)(hc->op_base + reg))
#define EHCI_OP_READ16(reg) (*(uint16_t*)(hc->op_base + reg))
#define EHCI_OP_READ32(reg) (*(uint32_t*)(hc->op_base + reg))

#define EHCI_CAP_READ8(reg) (*(uint8_t*)(hc->mmio_base + reg))
#define EHCI_CAP_READ16(reg) (*(uint16_t*)(hc->mmio_base + reg))
#define EHCI_CAP_READ32(reg) (*(uint32_t*)(hc->mmio_base + reg))

/* Lock */
static spinlock_t ehci_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EHCI", __VA_ARGS__)

/**
 * @brief EHCI scan method
 */
int ehci_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    // We know this device is of type 0x0C03, but it's only EHCI if the interface is 0x20
    if (pci_readConfigOffset(bus, slot, function, PCI_PROGIF_OFFSET, 1) == 0x20) {
        *((uint32_t*)data) = PCI_ADDR(bus, slot, function, 0x0);
        return 1; // Found it
    }
    
    return 0;
}

/**
 * @brief Allocate a new queue head
 * @param hc The host controller
 */
static ehci_qh_t *ehci_allocateQH(ehci_t *hc) {
    if (!hc) return NULL;

    ehci_qh_t *qh = (ehci_qh_t*)pool_allocateChunk(hc->qh_pool);
    if (!qh) {
        // Temporary
        kernel_panic_extended(OUT_OF_MEMORY, "ehci-qhpool", "*** No more memory remaining to allocate queue heads (KERNEL BUG)\n");
    }

    memset(qh, 0, sizeof(ehci_qh_t));
    qh->td_list = list_create("td list");   // TODO: This shouldnt be here
    qh->token.active = 1;

    LOG(DEBUG, "[QH:ALLOC] New QH created at %p/%p\n", qh, mem_getPhysicalAddress(NULL, (uintptr_t)qh));
    return qh;
}

/**
 * @brief Allocate a new transfer descriptor
 * @param hc The host controller
 */
static ehci_td_t *ehci_allocateTD(ehci_t *hc) {
    if (!hc) return NULL;

    ehci_td_t *td = (ehci_td_t*)pool_allocateChunk(hc->td_pool);
    if (!td) {
        // Temporary
        kernel_panic_extended(OUT_OF_MEMORY, "ehci-tdpool", "*** No more memory remaining to allocate transfer descriptors (KERNEL BUG)\n");
    }

    memset(td, 0, sizeof(ehci_td_t));

    LOG(DEBUG, "[TD:ALLOC] New TD created at %p/%p\n", td, mem_getPhysicalAddress(NULL, (uintptr_t)td));
    return td;
}

/**
 * @brief Create a queue head
 * @param hc The host controller
 * @param t The USB transfer currently going on
 * @param port The device port (leave as 0 if none)
 * @param hub_addr The USB hub address (leave as 0 if none)
 * @param transfer_type Transfer type, e.g. @c EHCI_TRANSFER_CONTROL
 * @param speed The speed of the transfer
 * @param address The address of the device
 * @param endpt The endpoint number
 * @param mps The max packet size
 * 
 * @returns Queue head object
 */
static ehci_qh_t *ehci_createQH(ehci_t *hc, USBTransfer_t *transfer, uint32_t port, uint32_t hub_addr, int transfer_type, int speed, uint32_t address, uint32_t endpt, uint32_t mps) {
    if (!hc) return NULL;

    // Allocate a queue head
    ehci_qh_t *qh = ehci_allocateQH(hc);
    qh->transfer = transfer;

    // Setup endpoint capabilities
    qh->cap.hub_addr = hub_addr;
    qh->cap.port = port;
    
    // Some special characteristics need to be updated on low/full speed devices
    if ((speed == USB_FULL_SPEED || speed == USB_LOW_SPEED)) {
        // TODO: Check endpoint to make sure its control
        if (transfer_type == EHCI_TRANSFER_CONTROL) {
            qh->ch.c = 1;           // Assuming control endoint
        } else {
            qh->cap.scm = 0x1C;     // Interrupt - complete on microframes 2, 3, or 4
        }
    }

    // Is this an interrupt transfer?
    if (transfer_type == EHCI_TRANSFER_INTERRUPT) {
        qh->cap.ism = 1;    // Update schedule mask
    } else {
        // No, setup NAK count reload
        qh->ch.rl = 5;
    }

    // Setup endpoint characteristics
    qh->ch.devaddr = address;
    qh->ch.eps = speed;
    qh->ch.mps = mps;
    qh->ch.dtc = 1;
    qh->ch.endpt = endpt;

    // We NEVER use td_current
    qh->td_current.terminate = 1;

    // Done!
    LOG(DEBUG, "[QH:SETUP] QH %p - transfer %p port 0x%x hubaddr 0x%x type %d speed %d devaddr 0x%x endpt 0x%x\n", qh, transfer, port, hub_addr, transfer_type, speed, address, endpt);
    return qh;

}

/**
 * @brief Allocate and create a new transfer descriptor
 * 
 * @param hc The host controller
 * @param speed The speed of the transfer descriptor
 * @param toggle Syncronization - see https://wiki.osdev.org/Universal_Serial_Bus#Data_Toggle_Synchronization
 * @param type The type of the packet, for example @c EHCI_PACKET_IN
 * @param length The length of the transfer, not including protocol bytes like PID/CRC
 * @param data The data of the transfer (usually a descriptor)
 *
 * @returns A new TD - make sure to link it to the prev one
 */
ehci_td_t *ehci_createTD(ehci_t *hc, int speed, uint32_t toggle, uint32_t type, uint32_t length, void *data) {
    if (!hc) return NULL;

    // Allocate a qTD
    ehci_td_t *td = ehci_allocateTD(hc);
    TD_LINK_TERM(td);

    // Setup token
    td->token.toggle = toggle;
    td->token.len = length;
    td->token.cerr = 3;
    td->token.pid = type;
    td->token.active = 1;
    
    // Data buffer. Note that for buffer #0 offsets can be used
    td->buffer[0] = (uint32_t)(uintptr_t)data;

#ifdef __ARCH_X86_64__
    td->ext_buffer[0] = (uint32_t)((uintptr_t)data >> 32);
#endif    

    // Configure other buffers
    uintptr_t data_aligned = (uintptr_t)data & ~0xFFF;

    for (int i = 1; i < 4; i++) {
        // Go up a page and setup buffers
        data_aligned += 0x1000;
        td->buffer[i] = (uint32_t)(uintptr_t)data;
#ifdef __ARCH_X86_64__
        td->ext_buffer[i] = (uint32_t)((uintptr_t)data >> 32);
#endif
    }

    // Done!
    LOG(DEBUG, "[TD:SETUP] New TD created at %p/%p - type 0x%x speed %i toggle 0x%x\n", td, LINK(td) << 5, type, speed, toggle);
    return td;
}

/**
 * @brief Destroy, unlink, and free a queue head. This frees all TDs as well
 * @param controller The controller
 * @param qh The queue head
 */
void ehci_destroyQH(USBController_t *controller, ehci_qh_t *qh) {
    ehci_t *hc = HC(controller);

    // We need to first unlink the queue head from the chain
    spinlock_acquire(&ehci_lock);
    node_t *node = list_find(hc->periodic_list, (void*)qh);
    
    if (!node) {
        // Check asyncronous
        node = list_find(hc->async_list, (void*)qh);
    }

    if (node) {
        // Unlink
        if (node->prev && node->prev->value) {
            ehci_qh_t *qh_prev = (ehci_qh_t*)node->prev->value;

            // Do we need to process the next queue head?
            if (!qh->qhlp.terminate) {
                // The list doesn't terminate, we have to link our next queue heads
                if (!node->next || !node->next->value) {
                    LOG(ERR, "Queue head does not terminate but the list does not contain a next queue head!\n");
                }
                ehci_qh_t *qh_next = (ehci_qh_t*)node->next->value;
                QH_LINK_QH(qh_prev, qh_next);
            } else {
                QH_LINK_TERM(qh_prev);
                qh_prev->qhlp.qhlp = 0;
            }

            // Delete from list
            if (node->owner == (void*)hc->async_list) {
                list_delete(hc->async_list, node);
            } else {
                list_delete(hc->periodic_list, node);
            }

            kfree(node);
        } else {
            // This means they tried to remove hc->async_qh or the base periodic QH
            // Not allowed - this means we'd have to update hc->frame_list or rewrite ASYNCLISTADDR
            LOG(WARN, "Possible attempted removal of root asyncronous/periodic QH. This is not supported - cannot restructure chain\n");
            return;
        }
    } else {
        LOG(WARN, "Tried to destroy queue head that is not apart of HC list\n");
    }

    // Zero out the td fields
    qh->td_next.terminate = 1;
    qh->td_next.lp = 0;

    // Each TD is allocated to a pool
    foreach(td_node, qh->td_list) {
        if (td_node->value) {
            ehci_td_t *td = (ehci_td_t*)td_node->value;
            LOG(DEBUG, "[TD:FREE] TD at %p destroyed\n", td);
            pool_freeChunk(hc->td_pool, (uintptr_t)td);
        }
    }

    list_destroy(qh->td_list, false);

    // Free the queue head
    LOG(DEBUG, "[QH:FREE] QH at %p destroyed\n", qh);
    pool_freeChunk(hc->qh_pool, (uintptr_t)qh);
    spinlock_release(&ehci_lock);
}

/**
 * @brief Write to a port
 * @param hc The host controller
 * @param port The port to write to
 * @param value The value to write
 * @todo Make this a macro
 */
void ehci_writePort(ehci_t *hc, uint32_t port, uint16_t data) {
    // First we have to read the port to make sure that we're not overwriting anything
    uint32_t current = EHCI_OP_READ32(port);
    current |= data;
    current &= ~(EHCI_PORTSC_CONNECT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE); // Clear the change flags
    current &= ~((1 << 5) | (1 << 4) | (1 << 0)); // Clear reserved
    EHCI_OP_WRITE32(port, current);
}

/**
 * @brief Clears a port's flags
 * @param hc The host controller
 * @param port The port to clear
 * @param flag The flag(s) to clear
 * @todo Make this a macro
 */
void ehci_clearPort(ehci_t *hc, uint32_t port, uint32_t data)  {
    uint32_t current = EHCI_OP_READ32(port);
    current &= ~(EHCI_PORTSC_CONNECT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE);  // Clear the change flags
    current &= ~data;
    current |= (EHCI_PORTSC_CONNECT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE) & data;    // Set the change flag if data changed one of these bits
    current &= ~((1 << 5) | (1 << 4) | (1 << 0)); // Clear reserved
    EHCI_OP_WRITE32(port, current);
}

/**
 * @brief Probe for available ports and initialize them
 * @param controller The USB controller to probe devices from
 * 
 * @returns The number of found devices
 */
int ehci_probe(USBController_t *controller) {
    ehci_t *hc = HC(controller);
    if (!hc) return 0;

    // Read port count
    uint32_t nports = EHCI_CAP_READ32(EHCI_REG_HCSPARAMS) & EHCI_HCSPARAMS_NPORTS;

    // Iterate through each port
    int initialized_ports = 0;
    for (uint32_t port = 0; port < nports; port++) {
        // Get the PORTSC address
        uint32_t port_addr = EHCI_REG_PORTSC + (port*sizeof(uint32_t));

        // Reset the port
        ehci_writePort(hc, port_addr, EHCI_PORTSC_RESET);
        clock_sleep(100);
        ehci_clearPort(hc, port_addr, EHCI_PORTSC_RESET);

        LOG(DEBUG, "EHCI resetting port 0x%x (status: %08x)\n", hc->op_base + port_addr, EHCI_OP_READ32(EHCI_REG_PORTSC + (port*4)));

        int port_enabled = 0;
        int port_connected = 0; // Full speed devices will connect but won't enable
        
        // Now we can wait ~200ms for port to enable (it's required to wait at elast 100ms) while checking the status
        uint16_t status;
        for (int i = 0; i < 20; i++) {
            clock_sleep(10);

            status = EHCI_OP_READ32(port_addr);

            if (!(status & EHCI_PORTSC_CONNECT)) {
                break; // No connection
            }

            port_connected = 1;

            if (status & (EHCI_PORTSC_CONNECT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE)) {
                // Acknowledge change in status
                ehci_clearPort(hc, port_addr, (EHCI_PORTSC_CONNECT_CHANGE | EHCI_PORTSC_ENABLE_CHANGE));
                continue;
            }

            // Low speed device?
            if (status & EHCI_PORTSC_LS) {
                // Yep - release this to the companion controller
                LOG(DEBUG, "Releasing low-speed device to companion controller.\n");
                ehci_writePort(hc, port_addr, EHCI_PORTSC_OWNER);
                port_connected = 0;
                port_enabled = 0;
                break;
            }

            // Has the port completed its enabling process?
            if (status & EHCI_PORTSC_ENABLE) {
                port_enabled = 1;
                break;
            }

            // Nope, enable the port.
            ehci_writePort(hc, port_addr, EHCI_PORTSC_ENABLE);
        }

        if (port_enabled && port_connected) {
            // The port was successfully enabled
            initialized_ports++;
            LOG(DEBUG, "Found an EHCI device connected to port %i\n", port);
        
            // Create a device (since we give up all the full/low speed devices they are always high)
            USBDevice_t *dev = usb_createDevice(controller, port, USB_HIGH_SPEED, ehci_control, ehci_interrupt);
            dev->mps = 64; // TODO: Bochs says to make this equal the mps corresponding to the speed of the device

            // Initialize the device
            if (usb_initializeDevice(dev)) {
                LOG(ERR, "Failed to initialize EHCI device\n");
                
                usb_destroyDevice(controller, dev);
                initialized_ports--;
                break;
            }
        } else if (port_connected) {
            // This is probably a full speed device. Let's give these to the companion controller.
            LOG(DEBUG, "Full-speed device connected - releasing to companion controller\n");
            EHCI_OP_WRITE32(port_addr, EHCI_OP_READ32(port_addr) | EHCI_PORTSC_OWNER); 
        }
    }

    LOG(INFO, "Successfully initialized %d devices\n", initialized_ports);
    return initialized_ports;
}


/**
 * @brief Wait for a queue head to complete its transfer
 * @param controller The controller
 * @param qh The queue head to watch
 * 
 * @returns Nothing, but sets transfer status to USB_TRANSFER_SUCCESS or USB_TRANSFER_FAILEDs
 */
void ehci_waitForQH(USBController_t *controller, ehci_qh_t *qh) {
    if (!controller || !qh || !controller->hc) return;
    USBTransfer_t *transfer = qh->transfer;

    // Did the token halt?
    if (qh->token.halted) {
        LOG(ERR, "EHCI controler detected a halted QH\n");
        transfer->status = USB_TRANSFER_FAILED;
    }

    // Did we make it to the end?
    if (qh->td_next.terminate) {
        LOG(INFO, "Successfully completed transfer\n");
        if (!qh->token.active) {
            if (qh->token.data_buffer) {
                LOG(ERR, "EHCI controller detected a data buffer error\n");
            }

            if (qh->token.babble) {
                LOG(ERR, "EHCI controller detected incessent yapping\n");
            }

            if (qh->token.transaction) {
                LOG(ERR, "EHCI controller detected transaction error\n");
            }

            if (qh->token.miss) {
                LOG(ERR, "EHCI controller detected a missed microframe\n");
            }

            transfer->status = USB_TRANSFER_SUCCESS;
        }
    }

    LOG(DEBUG, "Waiting for QH %p/%p - td_next %08x\n", qh, mem_getPhysicalAddress(NULL, (uintptr_t)qh), qh->td_next.raw);

    if (transfer->status != USB_TRANSFER_IN_PROGRESS) {
        // Clear transfer from queue
        qh->transfer = NULL;
        
        // TODO: Update toggle state?
        // TODO: Destroy QH

        ehci_destroyQH(controller, qh);
    }
}


/**
 * @brief EHCI control transfer method
 */
int ehci_control(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer) {
    if (!controller || !dev || !transfer || !controller->hc) return USB_TRANSFER_FAILED;
    ehci_t *hc = HC(controller);

    // A CONTROL transfer consists of 3 parts:
    // 1. A SETUP packet that details the transaction
    // 2. Some DATA packets that convey transfer->data in terms of dev->mps
    // 3. A STATUS packet to complete the transaction

    // First, create the queue head that will hold the packets/TDs
    ehci_qh_t *qh = ehci_createQH(hc, transfer, dev->port, 0, EHCI_TRANSFER_CONTROL, dev->speed, dev->address, transfer->endpoint, dev->mps);
    QH_LINK_TERM(qh);

    // Setup variables that will change on the TDs
    uint32_t toggle = 0; // Toggle bit

    // Create the SETUP transfer descriptor
    ehci_td_t *td_setup = ehci_createTD(hc, dev->speed, toggle, EHCI_PACKET_SETUP, 8, (void*)mem_getPhysicalAddress(NULL, (uintptr_t)transfer->req));
    QH_LINK_TD(qh, td_setup);

    qh->td_next_alt.raw = 1;
    qh->td_next.terminate = 0;
    qh->td_next.lp = LINK2(td_setup);

    // Now create the DATA descriptors. These need to be limited to dev->mps but not padded.
    uint8_t *buffer = (uint8_t*)transfer->data;
    uint8_t *buffer_end = (uint8_t*)((uintptr_t)transfer->data + (uintptr_t)transfer->length);
    ehci_td_t *last = td_setup;
    ehci_td_t *first = NULL;

    while (buffer < buffer_end) {
        uint32_t transaction_size = buffer_end - buffer;
        if (transaction_size > dev->mps) transaction_size = dev->mps;

        if (!transaction_size) break; // Nothing left
        
        // Now create the TD
        toggle ^= 1;
        ehci_td_t *td = ehci_createTD(hc, dev->speed, toggle, (transfer->req->bmRequestType & USB_RT_D2H) ? EHCI_PACKET_IN : EHCI_PACKET_OUT,
                                        transaction_size, (void*)mem_getPhysicalAddress(NULL, (uintptr_t)buffer));
        if (!first) first = td;
        TD_LINK_TD(qh, last, td);
        buffer += transaction_size;
        last = td;
    }

    // Now all we have to do is create a STATUS packet to complete the chain
    ehci_td_t *td_status = ehci_createTD(hc, dev->speed, 1, (transfer->req->bmRequestType & USB_RT_D2H) ? EHCI_PACKET_OUT : EHCI_PACKET_IN, 0, NULL);
    TD_LINK_TD(qh, last, td_status);
    TD_LINK_TERM(td_status);

    qh->td_current.raw = 0;
    qh->td_next_alt.raw = 0;

    // Insert it into the asyncronous chain
    spinlock_acquire(&ehci_lock);
    ehci_qh_t *current = (ehci_qh_t*)hc->async_list->head->value;
    
    LOG(DEBUG, "[QH:CONTROL] Link to QH %p/%p\n", current, LINK2(current) << 4);
    QH_LINK_QH(current, qh);
    list_append(hc->async_list, (void*)qh);
    spinlock_release(&ehci_lock);

    LOG(DEBUG, "[QH:CONTROL] Control transfer setup - QH with TDs %08x, %08x, %08x (c,n,a)\n", qh->td_current.raw, qh->td_next.raw, qh->td_next_alt.raw);

    // Wait for transfer to finish
    while (transfer->status == USB_TRANSFER_IN_PROGRESS) {
        ehci_waitForQH(controller, qh);
    }

    // Destroy queue head
    ehci_destroyQH(controller, qh);

    return transfer->status;
}

/**
 * @brief EHCI interrupt transfer method
 */
int ehci_interrupt(USBController_t *controller, USBDevice_t *dev, USBTransfer_t *transfer) {
    return USB_TRANSFER_FAILED;
}

/**
 * @brief EHCI IRQ handler
 */
int ehci_irq(void *context) {
    ehci_t *hc = (ehci_t*)(context);

    // Get status
    uint32_t status = EHCI_OP_READ32(EHCI_REG_USBSTS);

    // Check for results
    if (status & EHCI_USBSTS_USBINT) {
        LOG(INFO, "EHCI IRQ: Transfer finished successfully\n");
    }

    if (status & EHCI_USBSTS_USBERRINT) {
        LOG(ERR, "EHCI IRQ: Transfer error\n");
    }

    if (status & EHCI_USBSTS_FLR) {
        LOG(ERR, "EHCI IRQ: Frame list rollover\n");
    }

    if (status & EHCI_USBSTS_PCD) {
        LOG(INFO, "EHCI IRQ: Port change detected\n");
    }

    if (status & EHCI_USBSTS_HSE) {
        LOG(ERR, "EHCI IRQ: Host system error\n");
    }

    LOG(DEBUG, "STATUS = %08x\n", status);
    EHCI_OP_WRITE32(EHCI_REG_USBSTS, status);

    return 0;
}

/**
 * @brief Driver initialization method
 */
int driver_init(int argc, char **argv) {
    // Scan for the EHCI PCI device
    uint32_t ehci_device = 0xDEADBEEF;
    if (pci_scan(ehci_scan, (void*)&ehci_device, 0x0C03) == 0 || ehci_device == 0xDEADBEEF) {
        LOG(INFO, "No EHCI controller found\n");
        return 0;
    }

    LOG(DEBUG, "EHCI controller located\n");

    // Read in the PCI bar
    pci_bar_t *bar = pci_readBAR(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), 0);
    if (!bar) {
        LOG(ERR, "EHCI controller does not have BAR0 - false positive?\n");
        return 1;
    }

    if (!(bar->type == PCI_BAR_MEMORY32 || bar->type == PCI_BAR_MEMORY64)) {
        LOG(ERR, "EHCI controller BAR0 is not MMIO\n");
        return 1;
    }

    // Now we can configure the PCI command register.
    // Read it in and set flags
    uint16_t ehci_pci_command = pci_readConfigOffset(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), PCI_COMMAND_OFFSET, 2);
    ehci_pci_command &= ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_INTERRUPT_DISABLE); // Enable interrupts and disable I/O space
    ehci_pci_command |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE);
    pci_writeConfigOffset(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), PCI_COMMAND_OFFSET, (uint32_t)ehci_pci_command & 0xFFFF);

    // Map into memory space
    uintptr_t mmio_mapped = mem_mapMMIO(bar->address, bar->size);

    // Construct a host controller structure
    ehci_t *hc = kmalloc(sizeof(ehci_t));
    memset(hc, 0, sizeof(ehci_t));
    hc->mmio_base = mmio_mapped;
    hc->op_base = mmio_mapped + EHCI_CAP_READ8(EHCI_REG_CAPLENGTH);
    
    // Do alignment check
    if (sizeof(ehci_td_t) & 31 || sizeof(ehci_qh_t) & 31) {
        printf(COLOR_CODE_RED "Driver invalid for system hardware (please update ehci.h).\n");
        LOG(ERR, "You are missing the 32-byte alignment required for TDs/QHs\n");
        LOG(ERR, "Please modify the ehci.h header file to add some extra DWORDs and try again.\n");
        LOG(ERR, "Require a 32-byte alignment but QH = %d and TD = %d\n", sizeof(ehci_qh_t), sizeof(ehci_td_t));
        return 1;
    }

    // Allocate a frame list
    hc->frame_list = (ehci_flp_t*)mem_allocateDMA(PAGE_SIZE);
    memset(hc->frame_list, 0, PAGE_SIZE);
    LOG(DEBUG, "Frame list allocated to %p\n", hc->frame_list);

    // Create the pools
    hc->qh_pool = pool_create("ehci qh pool", sizeof(ehci_qh_t), 512 * sizeof(ehci_qh_t), 0, POOL_DMA);
    hc->td_pool = pool_create("ehci qtd pool", sizeof(ehci_td_t), 512 * sizeof(ehci_td_t), 0, POOL_DMA);

    // Create the queue head lists
    hc->periodic_list = list_create("ehci periodic qh list");
    hc->async_list = list_create("ehci async qh list");

    // Setup the periodic list
    ehci_qh_t *qh = ehci_allocateQH(hc);
    list_append(hc->periodic_list, (void*)qh);
    QH_LINK_TERM(qh);
    qh->td_next.terminate = 1;
    
    // The QH that we just created will serve as the periodic base
    // Depending on the transfer, a queue head can be added to the periodic queue (best for interrupt transfers) or the async queue (best for control transfers)

    // Make frame list skeleton
    for (int i = 0; i < 1024; i++) {
        hc->frame_list[i].type = EHCI_FLP_TYPE_QH;
        hc->frame_list[i].lp = LINK(qh);
        hc->frame_list[i].terminate = 0;
    }

    LOG(DEBUG, "hc->frame_list[0] = 0x%x\n", hc->frame_list[0]);

    // Mark entry 1024 as terminate
    hc->frame_list[1023].terminate = 1;

    // Allocate the asyncronous QH
    hc->qh_async = ehci_allocateQH(hc); 
    hc->qh_async->td_current.terminate = 1; // This is never used
    hc->qh_async->td_next.terminate = 1;    // No TD
    hc->qh_async->ch.h = 1;                 // Head of asyncronous list
    QH_LINK_TERM(hc->qh_async);             // Terminate the QH

    list_append(hc->async_list, (void*)hc->qh_async);

    // Register the interrupt handler
    uint8_t irq = pci_getInterrupt(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device));

    // Does it have an IRQ?
    if (irq == 0xFF) {
        LOG(ERR, "EHCI controller does not have interrupt number\n");
        LOG(ERR, "This is an implementation bug, halting system (REPORT THIS)\n");
        for (;;);
    }

    // Register IRQ handler
    hal_registerInterruptHandlerContext(irq, ehci_irq, (void*)hc);

    // We need to take over the controller. Read EECP
    uint32_t eecp = (EHCI_CAP_READ32(EHCI_REG_HCCPARAMS) & EHCI_HCCPARAMS_EECP) >> EHCI_HCCPARAMS_EECP_SHIFT;
    if (eecp >= 0x40) {
        uint32_t legsup = pci_readConfigOffset(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), eecp + USBLEGSUP, 4);

        if (legsup != PCI_NONE && legsup & USBLEGSUP_HC_BIOS) {
            LOG(INFO, "Legacy support indicates BIOS still owns EHCI controller - taking\n");

            pci_writeConfigOffset(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), eecp + USBLEGSUP, legsup | USBLEGSUP_HC_OS);
            
            // !!! please kill me
            int timeout = 2000;
            while (timeout) {
                uint32_t legsup = pci_readConfigOffset(PCI_BUS(ehci_device), PCI_SLOT(ehci_device), PCI_FUNCTION(ehci_device), eecp + USBLEGSUP, 4);
                if (!(legsup & USBLEGSUP_HC_BIOS) && (legsup & USBLEGSUP_HC_OS)) {
                    LOG(INFO, "EHCI controller owned\n");
                    break;
                }

                clock_sleep(10);
                timeout -= 10;
            }

            if (timeout <= 0) {
                LOG(ERR, "Failed to take ownership of EHCI controller. This could be a bug in the kernel. Trying to continue anyways.\n");
            }
        }
    }
    
    // Now reset the EHCI controller
    if (EHCI_OP_READ32(EHCI_REG_USBCMD) & EHCI_USBCMD_RS) {
        // Clear R/S bit as HCRESET cannot be set when running
        LOG(INFO, "Disabling R/S, USBCMD = %08x\n", EHCI_OP_READ32(EHCI_REG_USBCMD));
        EHCI_OP_WRITE32(EHCI_REG_USBCMD, (EHCI_OP_READ32(EHCI_REG_USBCMD) & ~(EHCI_USBCMD_RS)));
    }

    // TODO: Make sure HCHalted is set in USBSTS

    LOG(INFO, "Reset host controller now (USBCMD = %08x)\n", EHCI_OP_READ32(EHCI_REG_USBCMD));
    EHCI_OP_WRITE32(EHCI_REG_USBCMD, EHCI_USBCMD_HCRESET);
    while (1) {
        // while (EHCI_OP_READ32(EHCI_REG_USBCMD) & HCRESET) gets optimized or something by GCC, for some weird reason.
        uint32_t usbcmd = EHCI_OP_READ32(EHCI_REG_USBCMD);
        if (!(usbcmd & EHCI_USBCMD_HCRESET)) break;
        clock_sleep(5);
        LOG(DEBUG, "Host controller has not finished resetting - USBCMD = %08x\n", usbcmd);
    }
    LOG(INFO, "Reset host controller success\n");

    // Start the EHCI controller
    // See section 4.1 of the EHCI specification for further details

    // Program the CTRLDSSEGMENT with our segment
    EHCI_OP_WRITE32(EHCI_REG_CTRLDSSEGMENT, 0);
    
    // Enable interrupts we want
    EHCI_OP_WRITE32(EHCI_REG_USBINTR, EHCI_USBINTR_ERR | EHCI_USBINTR_HSE | EHCI_USBINTR_USBINT);

    // Setu periodic list base (+ async because why not)
    EHCI_OP_WRITE32(EHCI_REG_PERIODICLISTBASE, mem_getPhysicalAddress(NULL, (uintptr_t)hc->frame_list));
    EHCI_OP_WRITE32(EHCI_REG_ASYNCLISTADDR, mem_getPhysicalAddress(NULL, (uintptr_t)hc->qh_async));
    EHCI_OP_WRITE32(EHCI_REG_FRINDEX, 0);

    // Write USBCMD to have an ITC of 8, enable periodic/async scheduling, and run
    EHCI_OP_WRITE32(EHCI_REG_USBCMD, ((8 << EHCI_USBCMD_ITC_SHIFT) | EHCI_USBCMD_PSE | EHCI_USBCMD_ASE | EHCI_USBCMD_RS));

    // Wait for controller to spinup
    LOG(DEBUG, "Waiting for controller to start...\n");
    while (1) {
        // <ditto>
        asm ("" ::: "memory"); 
        uint32_t usbsts = EHCI_OP_READ32(EHCI_REG_USBSTS);
        asm ("" ::: "memory");
        
        if (!(usbsts & EHCI_USBSTS_HCHALTED)) break;
        clock_sleep(5);
        
    }

    // Set CF
    EHCI_OP_WRITE32(EHCI_REG_CONFIGFLAG, 1);
    clock_sleep(200);

    uint16_t hci_version = EHCI_CAP_READ16(EHCI_REG_HCIVERSION);
    LOG(INFO, "EHCI controller online - interface version %d.%d\n", (hci_version >> 8), (hci_version >> 4) & 0xF);

    // Create controller structure
    USBController_t *controller = usb_createController((void*)hc, NULL); // TODO: No polling method

    // Probe for devices
    // TODO: For the USB stack, make the main USB logic probe for devices
    ehci_probe(controller);

    // Register the controller
    usb_registerController(controller);

    return 0;
}

/**
 * @brief Driver deinitialization method
 */
int driver_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "EHCI Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};

