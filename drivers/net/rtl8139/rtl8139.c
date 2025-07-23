/**
 * @file drivers/net/rtl8139/rtl8139.c
 * @brief Realtek RTL8139 driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */


#include "rtl8139.h"
#include <kernel/drivers/pci.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/loader/driver.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:RTL8139", __VA_ARGS__)

/* Macros */
#define RTL8139_WRITE8(reg, value) rtl8139_write(nic, reg, value, 1)
#define RTL8139_WRITE16(reg, value) rtl8139_write(nic, reg, value, 2)
#define RTL8139_WRITE32(reg, value) rtl8139_write(nic, reg, value, 4)
#define RTL8139_READ8(reg) (uint8_t)rtl8139_read(nic, reg, 1)
#define RTL8139_READ16(reg) (uint16_t)rtl8139_read(nic, reg, 2)
#define RTL8139_READ32(reg) (uint32_t)rtl8139_read(nic, reg, 4)

/**
 * @brief RTL8139 write to offset
 */
void rtl8139_write(rtl8139_t *nic, uint16_t reg, uint32_t value, int size) {
    if (nic->io_space) {
        switch (size) {
            case 1:
                outportb((unsigned short)nic->mmio_addr + reg, (uint8_t)value);
                break;
            case 2:
                outportw((unsigned short)nic->mmio_addr + reg, (uint16_t)value);
                break;
            default:
                outportl((unsigned short)nic->mmio_addr + reg, (uint32_t)value);
                break;
        }
    } else {
        switch (size) {
            case 1:
                *((uint8_t*)(nic->mmio_addr + reg)) = (uint8_t)value;
                break;
            case 2:
                *((uint16_t*)(nic->mmio_addr + reg)) = (uint16_t)value;
                break;    
            default:
                *((uint32_t*)(nic->mmio_addr + reg)) = (uint32_t)value;
                break;
        }
    }
}

/**
 * @brief RTL8139 read from offset
 */
uint32_t rtl8139_read(rtl8139_t *nic, uint16_t reg, int size) {
    if (nic->io_space) {
        switch (size) {
            case 1:
                return inportb((unsigned short)nic->mmio_addr + reg);
                break;
            case 2:
                return inportw((unsigned short)nic->mmio_addr + reg);
                break;
            default:
                return inportl((unsigned short)nic->mmio_addr + reg);
        }
    } else {
        switch (size) {
            case 1:
                return *((uint8_t*)(nic->mmio_addr + reg));
            case 2:
                return *((uint16_t*)(nic->mmio_addr + reg));
            default:
                return *((uint32_t*)(nic->mmio_addr + reg));
        }
    }
}

/**
 * @brief Read the MAC address of the RTL8139
 * @param nic The nic to use
 * @param mac Where to read the MAC address to
 */
void rtl8139_readMAC(rtl8139_t *nic, uint8_t *mac) {
    // Get the MAC address
    uint32_t mac1 = RTL8139_READ32(RTL8139_REG_MAC);
    uint16_t mac2 = RTL8139_READ16(RTL8139_REG_MAC + 0x4);

    // Set the MAC address
    mac[0] = mac1 >> 0;
    mac[1] = mac1 >> 8;
    mac[2] = mac1 >> 16;
    mac[3] = mac1 >> 24;
    mac[4] = mac2 >> 0;
    mac[5] = mac2 >> 8;

    // Print it out
    LOG(DEBUG, "MAC: " MAC_FMT "\n", MAC(mac));
}

/**
 * @brief RTL8139 poll process
 */
void rtl8139_thread(void *context) {
    rtl8139_t *nic = (rtl8139_t*)context;
    
    for (;;) {
        // Put ourselves to sleep
        sleep_untilNever(current_cpu->current_thread);
        sleep_enter();

        // We've been woken up!
        // There must be packets around!
        // TODO: I really don't feel like writing all of those #defines again
        LOG(DEBUG, "Read packet (rx_current=0x%x)\n", nic->rx_current);
        uint16_t *packet = (uint16_t*)(nic->rx_buffer + nic->rx_current);
        uint16_t packet_len = packet[1];
        ethernet_handle((ethernet_packet_t*)(packet+2), nic->nic, packet_len);

        // Update counts
        NIC(nic->nic)->stats.rx_packets++;
        NIC(nic->nic)->stats.rx_bytes += packet_len;

        // Update rx_current
        nic->rx_current += (packet_len + 4 + 3) & ~3;
        if (nic->rx_current >= 8192) {
            nic->rx_current -= 8192;
        }

        // Update on device
        RTL8139_WRITE16(RTL8139_REG_CAPR, nic->rx_current - 16);

    }
}

/**
 * @brief RTL8139 IRQ handler
 */
int rtl8139_handler(void *context) {
    rtl8139_t *nic = (rtl8139_t*)context;
    if (nic) {
        // Update status
        uint16_t status = RTL8139_READ16(RTL8139_REG_ISR);
        RTL8139_WRITE16(RTL8139_REG_ISR, 0x05);

        if (status & RTL8139_ISR_TOK) {
            // Transmit ok
            // Free the memory
            mem_freeDMA(nic->tx_buffer, nic->tx_buffer_size);

            // Release the lock
            spinlock_release(&nic->lock);
        }

        if (status & RTL8139_ISR_TER) {
            // Error
            NIC(nic->nic)->stats.tx_dropped++;
        }

        if (status & RTL8139_ISR_ROK) {
            // this is real
            sleep_wakeup(nic->receive_proc->main_thread);
        }

        if (status & RTL8139_ISR_RER) {
            // Error
            NIC(nic->nic)->stats.rx_dropped++;
        }
    }

    return 0;
}

/**
 * @brief Write packet method
 */
ssize_t rtl8139_writePacket(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    // Move the data to a phytsically contiuous block of RAM
    // TODO: Properly do this. Can we have the IRQ handler send a PROPER success message back instead of using this garbage lock system?
    rtl8139_t *nic = (rtl8139_t*)NIC(node)->driver;
    spinlock_acquire(&nic->lock);

    // Prepare a block of memory
    nic->tx_buffer = mem_allocateDMA(size);
    nic->tx_buffer_size = size;
    memcpy((void*)nic->tx_buffer, buffer, size);

    // Send it
    uintptr_t phys = mem_getPhysicalAddress(NULL, nic->tx_buffer);
    if (phys > 0xFFFFFFFF) {
        LOG(ERR, "The RTL8139 requires that you have a 32-bit memory address\n");
        LOG(ERR, "This is a kernel bug. Report please!\n");
        mem_freeDMA(nic->tx_buffer, size);
        spinlock_release(&nic->lock);
        return 0;
    }


    RTL8139_WRITE32(RTL8139_REG_TSAD + (nic->tx_current*4), phys & 0xFFFFFFFF);
    RTL8139_WRITE32(RTL8139_REG_TSD + (nic->tx_current*4), size);

    nic->tx_current++;
    if (nic->tx_current > 3) nic->tx_current = 0;
    
    // Update counts
    NIC(nic->nic)->stats.tx_packets++;
    NIC(nic->nic)->stats.tx_bytes += size;

    // NOTE: Don't release the lock. The IRQ handler will
    return size;
}

/**
 * @brief Initialize a RTL8139 NIC
 */
int rtl8139_init(pci_device_t *device) {
    LOG(INFO, "Initializing a RTL8139 NIC (bus %d slot %d func %d)\n", device->bus, device->slot, device->function);
    
    // Step 1: Get bus mastering on
    uint16_t command = pci_readConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, 2);
    command |= PCI_COMMAND_BUS_MASTER;
    pci_writeConfigOffset(device->bus, device->slot, device->function, PCI_COMMAND_OFFSET, command, 2);

    // Allocate a RTL8139 structure
    rtl8139_t *nic = kzalloc(sizeof(rtl8139_t));
    nic->pci_device = device;
    
    // Get MMIO
    pci_bar_t *bar = pci_readBAR(device->bus, device->slot, device->function, 0);
    if (!bar) {
        LOG(ERR, "BAR0 does not exist or could not be read.\n");
        printf(COLOR_CODE_YELLOW "[RTL8139] BAR0 does not exist. Bad chip?");
        return 1;
    }

    // The RTL8139 is a rare case where I/O is actually in use. Determine the method of access.
    if (bar->type == PCI_BAR_IO_SPACE) {
        // I/O space, toggle the flag.
        nic->io_space = 1;
        nic->mmio_addr = bar->address; // ???: Cast?
    } else {
        nic->mmio_addr = bar->address;
    }

    LOG(DEBUG, "Communicating with this NIC over %s\n", (nic->io_space ? "I/O" : "MMIO"));

    // Register our IRQ handler
    uint8_t irq = pci_getInterrupt(nic->pci_device->bus, nic->pci_device->slot, nic->pci_device->function);

    if (irq == 0xFF || hal_registerInterruptHandler(irq, rtl8139_handler, (void*)nic)) {
        // Failed to register IRQ
        LOG(ERR, "Failed to register IRQ%d - trying MSI\n", irq);
        uint8_t msi = pci_enableMSI(nic->pci_device->bus, nic->pci_device->slot, nic->pci_device->function);
        if (msi == 0xFF || hal_registerInterruptHandler(msi, rtl8139_handler, (void*)nic)) {
            LOG(ERR, "No other configuration methods\n");
            goto _cleanup;
        }
    }

    // Read the MAC address
    uint8_t mac[6];
    rtl8139_readMAC(nic, mac);

    // Enable LWAKE + LWPTN
    // (I didn't care enough to document CONFIG1)
    RTL8139_WRITE8(0x52, 0x0);

    // Soft reset
    RTL8139_WRITE8(RTL8139_REG_CR, RTL8139_CMD_RST);
    while ((RTL8139_READ8(RTL8139_REG_CR) & RTL8139_CMD_RST)) {
        LOG(DEBUG, "Still waiting for RTL8139 to reset\n");
    }

    LOG(INFO, "RTL8139 reset successfully\n");

    // Allocate a receive buffer
    nic->rx_buffer = mem_allocateDMA(8192 + 16);
    memset((void*)nic->rx_buffer, 0, 8192 + 16);

    // Send it
    uintptr_t phys = mem_getPhysicalAddress(NULL, nic->rx_buffer);
    if (phys > 0xFFFFFFFF) {
        LOG(ERR, "The RTL8139 requires that you have a 32-bit memory address\n");
        LOG(ERR, "This is a kernel bug. Report please!\n");
        return 1;
    }

    RTL8139_WRITE32(RTL8139_REG_RBSTART, (uint32_t)(phys & 0xFFFFFFFF));

    // Now, configure IMR
    RTL8139_WRITE16(RTL8139_REG_IMR, RTL8139_IMR_ROK | RTL8139_IMR_TOK);

    // Setup RCR
    RTL8139_WRITE32(RTL8139_REG_RCR, RTL8139_RCR_AAP | RTL8139_RCR_AM | RTL8139_RCR_APM | RTL8139_RCR_AR | RTL8139_RCR_AER | RTL8139_RCR_WRAP);

    // Enable receive and transmitter
    RTL8139_WRITE8(RTL8139_REG_CR, RTL8139_CMD_RE | RTL8139_CMD_TE);

    // Create NIC
    nic->nic = nic_create("RTL8139", mac, NIC_TYPE_ETHERNET, (void*)nic);
    nic->nic->write = rtl8139_writePacket;
    NIC(nic->nic)->mtu = 1500;

    // Register it
    char name[128];
    snprintf(name, 128, "enp%ds%d", device->bus, device->slot);
    nic_register(nic->nic, name);

    // Start thread
    nic->receive_proc = process_createKernel("rtl8139 receiver", PROCESS_KERNEL, PRIORITY_MED, rtl8139_thread, (void*)nic);
    scheduler_insertThread(nic->receive_proc->main_thread);
    return 0;

_cleanup:
    if (nic && nic->rx_buffer) mem_freeDMA(nic->rx_buffer, 8192 + 16);
    if (nic) kfree(nic);
    return 0;
}

/**
 * @brief PCI scan method
 */
int rtl8139_scan(pci_device_t *dev, void *data) {
    return rtl8139_init(dev);
}

/**
 * @brief Init method
 */
int driver_init(int argc, char *argv[]) {
    pci_id_mapping_t id_list[] = {
        { .vid = 0x10ec, .pid = { 0x8193, PCI_NONE } },
        PCI_ID_MAPPING_END
    };


    pci_scan_parameters_t params = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = id_list
    };


    return pci_scanDevice(rtl8139_scan, &params, NULL);
}

/**
 * @brief Deinit method
 */
int driver_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "Realtek RTL8139 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};