/**
 * @file drivers/net/rtl8169/rtl8169.c
 * @brief RTL1869 network card driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/arch/arch.h>
#include "rtl8169.h"
#include <kernel/drivers/pci.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/loader/driver.h>
#include <kernel/debug.h>
#include <kernel/drivers/clock.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:RTL8169", __VA_ARGS__)


/* Macros */
#define RTL8169_WRITE8(reg, value) outportb(nic->base + reg, (value))
#define RTL8169_WRITE16(reg, value) outportw(nic->base + reg, (value))
#define RTL8169_WRITE32(reg, value) outportl(nic->base + reg, (value))
#define RTL8169_READ8(reg) (inportb(nic->base + reg))
#define RTL8169_READ16(reg) (inportw(nic->base + reg))
#define RTL8169_READ32(reg) (inportl(nic->base + reg))


/**
 * @brief Reset an RTL8169 NIC
 * @param nic The NIC to reset
 * @returns 0 on success, 1 on failure
 */
int rtl8169_reset(rtl8169_t *nic) {
    RTL8169_WRITE8(RTL8169_REG_CR, RTL8169_CR_RST);

    // Timeout
    uint64_t timeout = 100;
    while (timeout) {
        if (!(RTL8169_READ8(RTL8169_REG_CR) & RTL8169_CR_RST)) {
            break;
        }

        clock_sleep(1);
        timeout--;
    }

    // Timeout check
    if (timeout == 0) {
        LOG(ERR, "Resetting the RTL8169 NIC failed due to timeout\n");
        return 1;
    }

    LOG(INFO, "RTL8169 reset successfully\n");
    return 0;
}

/**
 * @brief Read the MAC address of the NIC
 * @param nic The NIC to read the MAC address of
 * @param mac Destination MAC
 */
int rtl8169_readMAC(rtl8169_t *nic, uint8_t *mac) {
    for (int i = 0; i < 6; i++) mac[i] = RTL8169_READ8(RTL8169_REG_IDR0 + i);
    return 0;
}

/**
 * @brief Initialize Rx registers for a NIC
 * @param nic The NIC to initialize Rx registers for
 * @returns 0 on success
 */
int rtl8169_initializeRx(rtl8169_t *nic) {
    // Create regions
    nic->rx_buffers = mem_allocateDMA(RTL8169_RX_DESC_COUNT * RTL8169_RX_BUFFER_SIZE);
    nic->rx_descriptors = mem_allocateDMA(RTL8169_RX_DESC_COUNT * sizeof(rtl8169_desc_t));

    LOG(DEBUG, "Rx buffers allocated to %p, descriptors allocated to %p\n", nic->rx_buffers, nic->rx_descriptors);

    // Start building each descriptor
    for (int i = 0; i < RTL8169_RX_DESC_COUNT; i++) {
        rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->rx_descriptors + (i * sizeof(rtl8169_desc_t)));

        desc->command = 0x1FF8 | RTL8169_DESC_CMD_OWN;
        if (i == RTL8169_RX_DESC_COUNT-1) desc->command |= RTL8169_DESC_CMD_EOR;

        // Get buffer
        uintptr_t buffer = mem_getPhysicalAddress(NULL, nic->rx_buffers + (i * RTL8169_RX_BUFFER_SIZE));

        // Setup remaining parameters
        desc->vlan = 0x00000000;
        desc->buffer_lo = (buffer & 0xFFFFFFFF);
        desc->buffer_hi = (buffer >> 32);
    }

    // Configure Rx descriptor addresses
    uintptr_t desc_phys = mem_getPhysicalAddress(NULL, nic->rx_descriptors);
    RTL8169_WRITE32(RTL8169_REG_RDSAR, desc_phys & 0xFFFFFFFF);
    RTL8169_WRITE32(RTL8169_REG_RDSAR + 4, desc_phys >> 32);

    // Enable 1024-byte MXDMA, unlimited RXFTH, accept physica lmatch, broadcast, multicast
    RTL8169_WRITE32(RTL8169_REG_RCR, RTL8169_RCR_MXDMA1024 | RTL8169_RCR_RXFTH_UNLIMITED | RTL8169_RCR_AB | RTL8169_RCR_AM | RTL8169_RCR_APM);

    // Configure MPS
    RTL8169_WRITE16(RTL8169_REG_RMS, 0x1FFF);

    // Initialized OK
    return 0;
}

/**
 * @brief Initialize Tx descriptors for a NIC
 * @param nic The NIC to initialize Tx registers for
 * @returns 0 on success
 */
int rtl8169_initializeTx(rtl8169_t *nic) {    
    // Create regions
    nic->tx_buffers = mem_allocateDMA(RTL8169_TX_DESC_COUNT * RTL8169_TX_BUFFER_SIZE);
    nic->tx_descriptors = mem_allocateDMA(RTL8169_TX_DESC_COUNT * sizeof(rtl8169_desc_t));

    LOG(DEBUG, "Tx buffers allocated to %p, descriptors allocated to %p\n", nic->tx_buffers, nic->tx_descriptors);

    // Start building each descriptor
    for (int i = 0; i < RTL8169_TX_DESC_COUNT; i++) {
        rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->tx_descriptors + (i * sizeof(rtl8169_desc_t)));

        // Get buffer
        uintptr_t buffer = mem_getPhysicalAddress(NULL, nic->tx_buffers + (i * RTL8169_TX_BUFFER_SIZE));

        // Setup parameters
        desc->command = (i == RTL8169_TX_DESC_COUNT-1) ? RTL8169_DESC_CMD_EOR : 0;
        desc->vlan = 0x00000000;
        desc->buffer_lo = (buffer & 0xFFFFFFFF);
        desc->buffer_hi = (buffer >> 32);
    }

    // Configure Tx descriptor addresses
    uintptr_t desc_phys = mem_getPhysicalAddress(NULL, nic->tx_descriptors);
    RTL8169_WRITE32(RTL8169_REG_TNPDS, desc_phys & 0xFFFFFFFF);
    RTL8169_WRITE32(RTL8169_REG_TNPDS + 4, desc_phys >> 32);

    // I ain't care enough to write the defines for this, enables standard IFG and 1024-byte DMA
    RTL8169_WRITE32(RTL8169_REG_TCR, (0x3 << 24) | (0x6 << 8));

    // Configure MPS
    RTL8169_WRITE16(RTL8169_REG_MTPS, 0x3B);

    return 0;
}

/**
 * @brief RTL8169 receive thread
 * @param context NIC
 */
void rtl8169_thread(void *context) {
    rtl8169_t *nic = (rtl8169_t*)context;
    
    for (;;) {
        // Sleep until forever
        sleep_untilNever(current_cpu->current_thread);
        int w = sleep_enter();

        if (w == WAKEUP_SIGNAL) {
            // Aw HELL nah
            LOG(ERR, "Thread exiting due to signal\n");  
            process_exit(current_cpu->current_process, 1); 
        }

        // Loop
        for (;;) {
            // Get descriptor
            rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->rx_descriptors + (nic->rx_current * sizeof(rtl8169_desc_t)));
            
            // Only descriptors no longer owned are valid
            if (desc->command & RTL8169_DESC_CMD_OWN) break;
        
            // Figure out packet length
            uint16_t pkt_length = desc->command & 0x3FFF;

            // Error?
            if (desc->command & (1 << 21)) {
                LOG(ERR, "Error in Rx descriptor\n");
                goto _next_desc;
            }

            // Update NIC statistics
            NIC(nic->nic)->stats.rx_bytes += pkt_length;
            NIC(nic->nic)->stats.rx_packets++;

            // Pass it on to the Ethernet handler
            ethernet_handle((ethernet_packet_t*)(nic->rx_buffers + (nic->rx_current * RTL8169_RX_BUFFER_SIZE)), nic->nic, pkt_length);

        _next_desc:
            nic->rx_current = (nic->rx_current + 1) % RTL8169_RX_DESC_COUNT;
            desc->command |= RTL8169_DESC_CMD_OWN;
        }
    }
}

/**
 * @brief RTL8169 IRQ handler
 * @param context NIC
 */
int rtl8169_irq(void *context) {
    rtl8169_t *nic = (rtl8169_t*)context;

    if (nic) {
        LOG(DEBUG, "Got IRQ on RTL8169\n");

        // Why were we interrupted?
        uint16_t isr = RTL8169_READ16(RTL8169_REG_ISR);
        RTL8169_WRITE16(RTL8169_REG_ISR, isr);

        if (isr & RTL8169_ISR_LINKCHG) {// Update link status
            if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_LINKSTS) {
                NIC(nic->nic)->state = NIC_STATE_UP;
            } else {
                NIC(nic->nic)->state = NIC_STATE_DOWN;
            }
            
            LOG(INFO, "Link status is now %s\n", (NIC(nic->nic)->state == NIC_STATE_UP) ? "UP" : "DOWN");
        }

        // Check for errors
        if (isr & RTL8169_ISR_RER) {
            NIC(nic->nic)->stats.rx_dropped++;
        }

        if (isr & RTL8169_ISR_TER) {
            NIC(nic->nic)->stats.tx_dropped++;
        }

        if (isr & RTL8169_ISR_TOK && nic->thr) {
            sleep_wakeup(nic->thr);
        }  

        // Did we get a packet?
        if (isr & RTL8169_ISR_ROK) {
            // Wake the thread up
            sleep_wakeup(nic->recv_proc->main_thread);
        }
    }

    return 0;
}

/**
 * @brief Get link speed as string
 * @param nic The NIC to get the link speed for
 */
char *rtl8169_link(rtl8169_t *nic) {
    if (!(RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_LINKSTS)) {
        return "DOWN";
    }

    if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_1000MF) {
        return "1000Mbps";
    } else if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_100M) {
        return "100Mbps";
    } else if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_10M) {
        return "10Mbps";
    } else {
        return "???";
    }
} 

/**
 * @brief Send a packet from an RTL8169 NIC
 * @param node Node
 * @param off Offset
 * @param size Size
 * @param buffer Buffer 
 */
ssize_t rtl8169_writePacket(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size) return 0;
    rtl8169_t *nic = (rtl8169_t*)NIC(node)->driver;

    spinlock_acquire(&nic->lock);

    // Get current descriptor
    rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->tx_descriptors + (nic->tx_current * sizeof(rtl8169_desc_t)));

    // Is the descriptor busy?
    if (desc->command & RTL8169_DESC_CMD_OWN) {
        sleep_untilNever(current_cpu->current_thread);
        nic->thr = current_cpu->current_thread;
        spinlock_release(&nic->lock);
        int w = sleep_enter();
        if (w == WAKEUP_SIGNAL) return -EINTR;
        spinlock_acquire(&nic->lock);
    }

    // The descriptor is ready, let's go
    uintptr_t tx_buffer = nic->tx_buffers +  (nic->tx_current * RTL8169_TX_BUFFER_SIZE);
    memcpy((void*)tx_buffer, buffer, size);

    // Give ownership
    desc->command = size | RTL8169_DESC_CMD_OWN | RTL8169_DESC_CMD_LS | RTL8169_DESC_CMD_FS;

    // Advance tx_current
    nic->tx_current = (nic->tx_current + 1) % RTL8169_TX_DESC_COUNT;
    if (nic->tx_current >= RTL8169_TX_DESC_COUNT-1) desc->command |= RTL8169_DESC_CMD_EOR;

    // Inform NIC gracefully
    RTL8169_WRITE8(RTL8169_REG_TPPoll, RTL8169_TPPoll_NPQ);

    NIC(node)->stats.tx_bytes += size;
    NIC(node)->stats.tx_packets++;

    spinlock_release(&nic->lock);
    return size;
}

/**
 * @brief Initialize a RTL8169 NIC
 * @param device The PCI device
 */
int rtl8169_init(pci_device_t *device) {
    LOG(DEBUG, "Initializing a RTL8169 NIC (bus %d slot %d func %d)\n", device->bus, device->slot, device->function);
    
    // Get BAR
    pci_bar_t *bar = pci_readBAR(device->bus, device->slot, device->function, 0);
    if (!bar) {
        LOG(ERR, "RTL8169 is lacking BAR0\n");
        return 1;
    }

    // Only I/O space BARs are supported
    if (bar->type != PCI_BAR_IO_SPACE) {
        LOG(ERR, "RTL8169 is not I/O space - kernel bug?\n");
        kfree(bar);
        return 1;
    }

    // Create RTL8169 NIC object
    rtl8169_t *nic = kzalloc(sizeof(rtl8169_t));
    nic->base = (uintptr_t)bar->address;
    kfree(bar);

    // Reset the NIC
    if (rtl8169_reset(nic)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    // Get the MAC address of the NIC
    uint8_t mac[6];
    if (rtl8169_readMAC(nic, mac)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    LOG(DEBUG, "MAC: " MAC_FMT "\n", MAC(mac));
    
    // Register IRQ handler
    uint8_t irq = pci_enableMSI(device->bus, device->slot, device->function);
    if (irq == 0xFF) {
        LOG(WARN, "RTL8169 does not support MSI, fallback to pin interrupt\n");
        irq = pci_getInterrupt(device->bus, device->slot, device->function);

        if (irq == 0xFF) {
            LOG(ERR, "No IRQ found for RTL8169 (or it needs to allocate one and we don't do that)\n");
            return 1;       
        } else if (hal_registerInterruptHandlerContext(irq, rtl8169_irq, (void*)nic)) {
            LOG(ERR, "Failed to register IRQ%d\n", irq);
            return 1;
        }
    } else {
        if (hal_registerInterruptHandlerContext(irq, rtl8169_irq, (void*)nic)) {
            LOG(ERR, "Failed to register IRQ%d\n", irq);
            return 1;
        }
    }

    LOG(DEBUG, "Registered IRQ%d for NIC\n", irq);

    // Enable configuration registers
    RTL8169_WRITE8(RTL8169_REG_9346CR, RTL8169_9346CR_MODE_CONFIG);

    // Initialize Rx
    if (rtl8169_initializeRx(nic)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    // Initialize Tx
    if (rtl8169_initializeTx(nic)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    // Enable receive and transmit
    RTL8169_WRITE8(RTL8169_REG_CR, RTL8169_CR_RE | RTL8169_CR_TE);

    // Enable interrupts
    RTL8169_WRITE16(RTL8169_REG_IMR, RTL8169_IMR_ROK | RTL8169_IMR_RER | RTL8169_IMR_TOK | RTL8169_IMR_TER | RTL8169_IMR_RDU | RTL8169_IMR_LINKCHG | RTL8169_IMR_FOVW | RTL8169_IMR_TDU);
    RTL8169_WRITE16(RTL8169_REG_ISR, 0xFFFF);

    // Create NIC object
    nic->nic = nic_create("rtl8169", mac, NIC_TYPE_ETHERNET, (void*)nic);
    
    // Link speed
    LOG(INFO, "Link speed: %s\n", rtl8169_link(nic));

    // Update link status
    if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_LINKSTS) {
        NIC(nic->nic)->state = NIC_STATE_UP;
    } else {
        NIC(nic->nic)->state = NIC_STATE_DOWN;
    }

    // Make receive kernel thread
    nic->recv_proc = process_createKernel("rtl8169 receiever", PROCESS_KERNEL, PRIORITY_LOW, rtl8169_thread, (void*)nic);
    scheduler_insertThread(nic->recv_proc->main_thread);
    
    nic->nic->write = rtl8169_writePacket;


    // Set MTU
    NIC(nic->nic)->mtu = 1500;

    // Mount the NIC!
    char name[128];
    snprintf(name, 128, "enp%ds%d", device->bus, device->slot);
    nic_register(nic->nic, name);

    return 0;
}

// /**
//  * @brief PCI scan method
//  */
// int rtl8169_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
//     if (vendor_id == 0x10ec && (device_id == 0x8161 || device_id == 0x8168 || device_id == 0x8169 || device_id == 0x2600)) {
//         // RTL8169
//         return rtl8169_init(PCI_ADDR(bus, slot, function, 0));
//     }

//     if ((vendor_id == 0x1259 && device_id == 0xc107) || (vendor_id == 0x1737 && device_id == 0x1032) || (vendor_id == 0x16ec && device_id == 0x0116)) {
//         return rtl8169_init(PCI_ADDR(bus, slot, function, 0));
//     }

//     return 0;
// }

/**
 * @brief PCI scan method
 */
int rtl8169_find(pci_device_t *dev, void *data) {
    return rtl8169_init(dev);
}

/**
 * @brief Init method
 */
int driver_init(int argc, char *argv[]) {
    pci_id_mapping_t id_list[] = {
        { .vid = 0x10ec, .pid = { 0x8161, 0x8168, 0x8169, 0x2600, PCI_NONE } },
        { .vid = 0x1259, .pid = { 0xc107, PCI_NONE } },
        { .vid = 0x1737, .pid = { 0x1032, PCI_NONE } },
        { .vid = 0x16ec, .pid = { 0x0116, PCI_NONE } },
        PCI_ID_MAPPING_END
    };

    pci_scan_parameters_t params = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = id_list
    };

    pci_scanDevice(rtl8169_find, &params, NULL);

    return DRIVER_STATUS_SUCCESS;
}

/**
 * @brief Deinit method
 */
int driver_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "Realtek RTL8169 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};