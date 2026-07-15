/**
 * @file drivers/net/rtl8169/rtl8169.c
 * @brief RTL1869 network card driver
 * 
 * Adapted from RTL8139 and most of the configs sourced from the banan-os driver:
 * https://github.com/Bananymous/banan-os/blob/0e0d7016b3e00dad8c10bc3825b87e9c3e8d39d2/kernel/kernel/Networking/RTL8169/RTL8169.cpp
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
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/loader/driver.h>
#include <kernel/debug.h>
#include <kernel/drivers/clock.h>
#include <kernel/misc/waitqueue.h>
#include <kernel/task/sleep.h>
#include <errno.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:RTL8169", __VA_ARGS__)


/* Macros */
#define RTL8169_WRITE8(reg, value) outportb(nic->base + reg, (value))
#define RTL8169_WRITE16(reg, value) outportw(nic->base + reg, (value))
#define RTL8169_WRITE32(reg, value) outportl(nic->base + reg, (value))
#define RTL8169_READ8(reg) (inportb(nic->base + reg))
#define RTL8169_READ16(reg) (inportw(nic->base + reg))
#define RTL8169_READ32(reg) (inportl(nic->base + reg))

/* Operations */
static ssize_t rtl8169_send(nic_t *nnic, size_t size, char *buffer);
static nic_ops_t rtl8169_nic_ops = {
    .send = rtl8169_send,
    .ioctl = NULL,
};

static uint32_t rtl8169_rxDescriptorCommand(int index) {
    uint32_t command = RTL8169_RX_DESC_BUFSIZE | RTL8169_DESC_CMD_OWN;
    if (index == RTL8169_RX_DESC_COUNT - 1) command |= RTL8169_DESC_CMD_EOR;
    return command;
}

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
    // Create descriptors, this should only alloc one page
    nic->rx_descriptors = dma_map(RTL8169_RX_DESC_COUNT * sizeof(rtl8169_desc_t));

    // Clear the RX descriptors
    memset((void*)nic->rx_descriptors, 0, RTL8169_RX_DESC_COUNT * sizeof(rtl8169_desc_t));

    // Start building each descriptor
    for (int i = 0; i < RTL8169_RX_DESC_COUNT; i++) {
        rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->rx_descriptors + (i * sizeof(rtl8169_desc_t)));

        // Get buffer
        nic->rx_buffers[i] = dma_map(RTL8169_RX_BUFFER_SIZE);
        uintptr_t buffer = arch_mmu_physical(NULL, nic->rx_buffers[i]);

        // Setup remaining parameters
        desc->vlan = 0x00000000;
        desc->buffer_lo = (buffer & 0xFFFFFFFF);
        desc->buffer_hi = (buffer >> 32);
        desc->command = rtl8169_rxDescriptorCommand(i);
    }

    // Configure Rx descriptor addresses
    uintptr_t desc_phys = arch_mmu_physical(NULL, nic->rx_descriptors);
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
    // Should be one page
    nic->tx_descriptors = dma_map(RTL8169_TX_DESC_COUNT * sizeof(rtl8169_desc_t));

    // Zero the descriptors initially
    memset((void*)nic->tx_descriptors, 0, RTL8169_TX_DESC_COUNT * sizeof(rtl8169_desc_t));

    // Start building each descriptor
    for (int i = 0; i < RTL8169_TX_DESC_COUNT; i++) {
        rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->tx_descriptors + (i * sizeof(rtl8169_desc_t)));

        // Get buffer
        nic->tx_buffers[i] = dma_map(RTL8169_TX_BUFFER_SIZE);
        uintptr_t buffer = arch_mmu_physical(NULL, nic->tx_buffers[i]);

        // Setup parameters
        desc->vlan = 0x00000000;
        desc->buffer_lo = (buffer & 0xFFFFFFFF);
        desc->buffer_hi = (buffer >> 32);
        desc->command = (i == RTL8169_TX_DESC_COUNT-1) ? RTL8169_DESC_CMD_EOR : 0;
    }

    // Configure Tx descriptor addresses
    uintptr_t desc_phys = arch_mmu_physical(NULL, nic->tx_descriptors);
    RTL8169_WRITE32(RTL8169_REG_TNPDS, desc_phys & 0xFFFFFFFF);
    RTL8169_WRITE32(RTL8169_REG_TNPDS + 4, desc_phys >> 32);

    // I ain't care enough to write the defines for this, enables standard IFG and 1024-byte DMA
    RTL8169_WRITE32(RTL8169_REG_TCR, (0x3 << 24) | (0x6 << 8));

    // Configure MPS
    RTL8169_WRITE16(RTL8169_REG_MTPS, 0x3B);

    return 0;
}

static int rtl8169_receivePackets(rtl8169_t *nic) {
    int packets = 0;

    for (;;) {
        // Get descriptor
        rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->rx_descriptors + (nic->rx_current * sizeof(rtl8169_desc_t)));
        uint32_t command = desc->command;
            
        // Only descriptors no longer owned are valid
        if (command & RTL8169_DESC_CMD_OWN) break;
        
        // Figure out packet length
        uint16_t pkt_length = command & 0x3FFF;

        // Error?
        if (command & (1 << 21)) {
            LOG(ERR, "Error in Rx descriptor\n");
            nic->n->stats.rx_dropped++;
            goto _next_desc;
        }

        // Update NIC statistics
        nic->n->stats.rx_bytes += pkt_length;
        nic->n->stats.rx_packets++;

        // Pass it on to the Ethernet handler
        ethernet_handle((ethernet_packet_t*)(nic->rx_buffers[nic->rx_current]), nic->n, pkt_length);

    _next_desc:
        desc->vlan = 0x00000000;
        desc->command = rtl8169_rxDescriptorCommand(nic->rx_current);
        nic->rx_current = (nic->rx_current + 1) % RTL8169_RX_DESC_COUNT;
        packets++;
    }

    return packets;
}

/**
 * @brief RTL8169 receive thread
 * @param context NIC
 */
void rtl8169_thread(void *context) {
    rtl8169_t *nic = (rtl8169_t*)context;

    for (;;) {
        wait_queue_node_t n;
        waitqueue_add(&nic->rx_waiters, &n);
        while (rtl8169_receivePackets(nic));
        waitqueue_wait(&nic->rx_waiters, &n, -1);
        waitqueue_remove(&nic->rx_waiters, &n);
    }
}

/**
 * @brief RTL8169 IRQ handler
 * @param context NIC
 */
int rtl8169_irq(irq_t *irq, void *context) {
    rtl8169_t *nic = (rtl8169_t*)context;

    // Why were we interrupted?
    uint16_t isr = RTL8169_READ16(RTL8169_REG_ISR);

    bool signal_tx = false;
    bool signal_rx = false;

    while (isr) {
        RTL8169_WRITE16(RTL8169_REG_ISR, isr);

        if ((isr & RTL8169_ISR_LINKCHG) && nic->n) {
            // Update link status
            if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_LINKSTS) {
                nic->n->state = NIC_STATE_UP;
            } else {
                nic->n->state = NIC_STATE_DOWN;
            }

            // 10ec:2600, Killer E2600: doesnt re-enable RE and TE on LinkCHG
            uint8_t cr = RTL8169_READ8(RTL8169_REG_CR);
            if ((cr & (RTL8169_CR_RE | RTL8169_CR_TE)) != (RTL8169_CR_RE | RTL8169_CR_TE)) {
                cr |= RTL8169_CR_RE | RTL8169_CR_TE;
                RTL8169_WRITE8(RTL8169_REG_CR, cr);
            }
            
            LOG(INFO, "Link status is now %s\n", (nic->n->state == NIC_STATE_UP) ? "UP" : "DOWN");
        }

        // Check for errors
        if ((isr & (RTL8169_ISR_RER | RTL8169_ISR_FOVW)) && nic->n) {
            nic->n->stats.rx_dropped++;
        }

        if ((isr & (RTL8169_ISR_TER))) {
            nic->n->stats.tx_dropped++;
            signal_tx = true;
        }

        if ((isr & (RTL8169_ISR_TOK))) {
            nic->n->stats.tx_packets++;
            signal_tx = true;
        }  

        // Did we get a packet?
        if ((isr & (RTL8169_ISR_ROK | RTL8169_ISR_FOVW | RTL8169_ISR_RDU))) {
            signal_rx = true;
        }

        isr = RTL8169_READ16(RTL8169_REG_ISR);
    }

    // TODO MAKE THIS LOCKLESS! Spinlocks in IRQ is NEVER a good thing
    if (signal_rx || signal_tx) {
        spinlock_acquire(&nic->lock);
        if (signal_rx) {
            waitqueue_wakeup(&nic->rx_waiters, 1);
        }

        if (signal_tx) {
            waitqueue_wakeup(&nic->tx_waiters, 1);
        }
        spinlock_release(&nic->lock);
    }

    // TODO: IRQ_NOT_SOURCE
    return IRQ_HANDLED;
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
static ssize_t rtl8169_send(nic_t *nnic, size_t size, char *buffer) {
    if (!size) return 0;
    if (size > RTL8169_TX_BUFFER_SIZE) return -EMSGSIZE;
    rtl8169_t *nic = (rtl8169_t*)nnic->driver;

    spinlock_acquire(&nic->lock);

    // Get current descriptor
    rtl8169_desc_t *desc = (rtl8169_desc_t*)(nic->tx_descriptors + (nic->tx_current * sizeof(rtl8169_desc_t)));

    // Is the descriptor busy?
    while (desc->command & RTL8169_DESC_CMD_OWN) {
        wait_queue_node_t n;
        waitqueue_add(&nic->tx_waiters, &n);
        spinlock_release(&nic->lock);
        int w = waitqueue_wait(&nic->tx_waiters, &n, -1);
        if (w != 0) {
            return w;
        }
        waitqueue_remove(&nic->tx_waiters, &n);
        spinlock_acquire(&nic->lock);

        desc = (rtl8169_desc_t*)(nic->tx_descriptors + (nic->tx_current * sizeof(rtl8169_desc_t)));
    }

    nic->thr = NULL;

    // The descriptor is ready, let's go
    uintptr_t tx_buffer = nic->tx_buffers[nic->tx_current];
    memcpy((void*)tx_buffer, buffer, size);

    // Give ownership
    uint32_t command = (size & 0x1FFF) | RTL8169_DESC_CMD_OWN | RTL8169_DESC_CMD_LS | RTL8169_DESC_CMD_FS;
    if (nic->tx_current == RTL8169_TX_DESC_COUNT - 1) command |= RTL8169_DESC_CMD_EOR;
    desc->command = command;

    // Advance tx_current
    nic->tx_current = (nic->tx_current + 1) % RTL8169_TX_DESC_COUNT;

    // Inform NIC gracefully
    RTL8169_WRITE8(RTL8169_REG_TPPoll, RTL8169_TPPoll_NPQ);

    nnic->stats.tx_bytes += size;

    spinlock_release(&nic->lock);
    return size;
}

/**
 * @brief Initialize a RTL8169 NIC
 * @param device The PCI device
 */
int rtl8169_init(pci_device_t *device) {
    LOG(DEBUG, "Initializing a RTL8169 NIC (bus %d slot %d func %d)\n", device->bus, device->slot, device->function);
    LOG(DEBUG, "VID=%04x DEVID=%04x\n", device->vid, device->devid);

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

    // Enable I/O + bus master space
    uint16_t cmd;
    pci_readConfigWord(device, PCI_COMMAND_OFFSET, &cmd);
    cmd |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_IO_SPACE;
    cmd &= ~(PCI_COMMAND_MEMORY_SPACE);
    pci_writeConfigWord(device, PCI_COMMAND_OFFSET, cmd);

    // Create RTL8169 NIC object
    rtl8169_t *nic = kzalloc(sizeof(rtl8169_t));
    nic->base = (uintptr_t)bar->address;
    SPINLOCK_INIT(&nic->lock);
    WAIT_QUEUE_INIT(&nic->tx_waiters);
    WAIT_QUEUE_INIT(&nic->rx_waiters);
    kfree(bar);

    // Disable C+ mode
    // TODO: Seems to be causing some problems?
    // RTL8169_WRITE16(0xE0, 0x0000);

    // Reset the NIC
    if (rtl8169_reset(nic)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    // Disable IRQs
    RTL8169_WRITE16(RTL8169_REG_IMR, 0);
    RTL8169_WRITE16(RTL8169_REG_ISR, 0xFFFF);

    // Get the MAC address of the NIC
    uint8_t mac[6];
    if (rtl8169_readMAC(nic, mac)) {
        LOG(ERR, "Error while initializing RTL8169\n");
        kfree(nic);
        return 1;
    }

    LOG(DEBUG, "MAC: " MAC_FMT "\n", MAC(mac));
    
    // Register IRQ handler. The device IMR stays masked until the NIC object and
    // receive thread are ready, so early interrupts cannot dereference them.
    int r = pci_allocateInterrupts(device, 1, 1, PCI_IRQ_MSI);
    if (r < 1) {
        LOG(ERR, "RTL8169 failed to allocate interrupts\n");
        return 1;
    }

    pci_irq_t *irq = pci_getInterruptVector(device, 0);
    irq_register(irq->vector, rtl8169_irq, IRQ_FLAG_DEFAULT, (void*)nic, NULL);


    LOG(DEBUG, "Registered IRQ%d for NIC\n", irq->vector);

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

    // Create NIC object
    char name[128];
    snprintf(name, 128, "enp%ds%d", device->bus, device->slot);
    nic->n = nic_create(name, NIC_TYPE_ETHERNET, &rtl8169_nic_ops, mac, (void*)nic);

    // Link speed
    LOG(INFO, "Link speed: %s\n", rtl8169_link(nic));

    // Update link status
    if (RTL8169_READ8(RTL8169_REG_PHYStatus) & RTL8169_PHYStatus_LINKSTS) {
        nic->n->state = NIC_STATE_UP;
    } else {
        nic->n->state = NIC_STATE_DOWN;
    }

    // Make receive kernel thread
    nic->recv_proc = process_createKernel("rtl8169 receiver", PROCESS_KERNEL, PRIORITY_LOW, rtl8169_thread, (void*)nic);
    scheduler_insertThread(nic->recv_proc->main_thread);

    // Set MTU
    nic->n->mtu = 1500;

    // Enable receive and transmit
    RTL8169_WRITE8(RTL8169_REG_CR, RTL8169_CR_RE | RTL8169_CR_TE);

    // Clear pending status after setup, then unmask the interrupts we handle.
    RTL8169_WRITE16(RTL8169_REG_ISR, 0xFFFF);
    RTL8169_WRITE16(RTL8169_REG_IMR, RTL8169_IMR_ROK | RTL8169_IMR_RER | RTL8169_IMR_TOK | RTL8169_IMR_TER | RTL8169_IMR_RDU | RTL8169_IMR_LINKCHG | RTL8169_IMR_FOVW | RTL8169_IMR_TDU);

    // Lock config
    RTL8169_WRITE8(RTL8169_REG_9346CR, 0);

    return 0;
}


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
        { .vid = 0x10ec, .devid = { 0x8161, 0x8168, 0x8169, 0x2600, PCI_NONE } },
        { .vid = 0x1259, .devid = { 0xc107, PCI_NONE } },
        { .vid = 0x1737, .devid = { 0x1032, PCI_NONE } },
        { .vid = 0x16ec, .devid = { 0x0116, PCI_NONE } },
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
