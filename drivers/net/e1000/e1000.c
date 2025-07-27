/**
 * @file drivers/net/e1000.c
 * @brief E1000 NIC driver
 * 
 * @todo Cleanup
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#pragma GCC diagnostic ignored "-Wunused-value"

#include "e1000.h"
#include <kernel/drivers/net/nic.h>
#include <kernel/drivers/clock.h>
#include <kernel/loader/driver.h>
#include <kernel/task/process.h>
#include <kernel/drivers/pci.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <string.h>

/* HAL */
#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

/* MMIO write methods */
#define E1000_WRITE8(reg, value) (*((uint8_t*)(nic->mmio + reg)) = (uint8_t)value)
#define E1000_WRITE16(reg, value) (*((uint16_t*)(nic->mmio + reg)) = (uint16_t)value)
#define E1000_WRITE32(reg, value) (*((uint32_t*)(nic->mmio + reg)) = (uint32_t)value)

/* MMIO read methods */
#define E1000_READ8(reg) (*((uint8_t*)(nic->mmio + reg)))
#define E1000_READ16(reg) (*((uint16_t*)(nic->mmio + reg)))
#define E1000_READ32(reg) (*((uint32_t*)(nic->mmio + reg)))

/* Command method (legacy code used direct macros so this helps to avoid optimization casualties) */
#define E1000_SENDCMD(reg, value) e1000_sendCommand(nic, reg, value)
#define E1000_RECVCMD(reg) e1000_receiveCommand(nic, reg)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:E1000", __VA_ARGS__)


/**
 * @brief Send a command to the nic
 * @param nic The nic to send to
 * @param reg The register to send the command to
 * @param value The value to write
 */
static void e1000_sendCommand(e1000_t *nic, uint16_t reg, uint32_t value) {
    E1000_WRITE32(reg, value);
}

/**
 * @brief Receive a command from the nic
 * @param nic The nic to get from
 * @param reg The register to read from
 */
static uint32_t e1000_receiveCommand(e1000_t *nic, uint16_t reg) {
    uint32_t value = E1000_READ32(reg);
    return value;
}

/**
 * @brief EEPROM detection
 * @param nic NIC
 */
int e1000_detectEEPROM(e1000_t *nic) {
    // TODO: Certain models do not support EEPROM, don't waste time.
    E1000_SENDCMD(E1000_REG_EEPROM, 1); // EEPROM reads a word at EE_ADDR and stores in EE_DATA

    // Wait for the DONE bit to be set
    for (int i = 0; i < 2000; i++) {
        uint32_t eeprom = E1000_RECVCMD(E1000_REG_EEPROM);
        if (eeprom & 0x10) return 1;
        LOG(DEBUG, "EEPROM detect %08x\n", eeprom);
    }

    return 0;
}

/**
 * @brief Read from EEPROM
 * @param nic The NIC
 * @param addr EEPROM address
 */
uint32_t e1000_readEEPROM(e1000_t *nic, uint8_t addr) {
    E1000_SENDCMD(E1000_REG_EEPROM, (1) | ((uint32_t)addr << 8));

    int timeout = 1000;
    while (timeout) {
        uint32_t tmp = E1000_RECVCMD(E1000_REG_EEPROM);
        if (tmp & (1 << 4)) {
            // Done!
            return ((uint16_t)((tmp >> 16) & 0xFFFF));
        }

        // Don't waste too much time
        clock_sleep(50);
        timeout -= 50;
    }
    

    return 0; // Unreachable
}

/**
 * @brief Read the MAC address from the NIC
 * @param nic The NIC to read MAC from
 * @param mac The output MAC
 */
int e1000_readMAC(e1000_t *nic, uint8_t *mac) {
    // Do we have an eeprom?
    if (nic->eeprom) {
        // Yes, use that!
        for (int i = 0; i < 6; i += 2) {
            uint32_t dword = e1000_readEEPROM(nic, i/2);
            mac[i] = (dword & 0xFF);
            mac[i+1] = (dword >> 8) & 0xFF;
        }

        // Because we read from EEPROM, remember to also set RXADDR
        uint32_t lo;
        memcpy(&lo, &mac[0], 4);

        uint32_t hi = 0;
        memcpy(&hi, &mac[4], 2);

        hi |= 0x80000000;

        E1000_WRITE32(E1000_REG_RXADDR, lo);
        E1000_WRITE32(E1000_REG_RXADDR, hi);
    } else {
        // Read from RXADDR
        uint32_t mac_low = E1000_READ32(E1000_REG_RXADDR);
        uint32_t mac_high = E1000_READ32(E1000_REG_RXADDRHIGH);

        // Put in MAC array
        mac[0] = (mac_low >> 0) & 0xFF;
        mac[1] = (mac_low >> 8) & 0xFF;
        mac[2] = (mac_low >> 16) & 0xFF;
        mac[3] = (mac_low >> 24) & 0xFF;
        mac[4] = (mac_high >> 0) & 0xFF;
        mac[5] = (mac_high >> 8) & 0xFF;
    }

    // All done
    return 0;
}

/**
 * @brief Initialize Tx descriptors
 */
void e1000_txinit(e1000_t *nic) {
    // First, allocate our Tx descriptors
    nic->tx_descs = (volatile e1000_tx_desc_t*)mem_allocateDMA(sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);
    memset((void*)nic->tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);

    // Now we actually need to allocate our Tx descriptors (i.e. setting up their bits, addresses, etc)
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        // Map in an address
        nic->tx_virt[i] = mem_allocateDMA(PAGE_SIZE); // TODO: Are we sure about this?
        nic->tx_descs[i].addr = mem_getPhysicalAddress(NULL, nic->tx_virt[i]);
        memset((void*)nic->tx_virt[i], 0, PAGE_SIZE);

        // Mark as EOP
        nic->tx_descs[i].status = 0;
        nic->tx_descs[i].cmd = E1000_CMD_EOP;
    } 

    // Write these addresses into the NIC
    uintptr_t phys_tx = mem_getPhysicalAddress(NULL, (uintptr_t)nic->tx_descs);
    E1000_SENDCMD(E1000_REG_TXDESCHI, (uint32_t)((uint64_t)phys_tx >> 32));
    E1000_SENDCMD(E1000_REG_TXDESCLO, (uint32_t)((uint64_t)phys_tx & 0xFFFFFFFF));

    // Setup length of descriptors
    E1000_SENDCMD(E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));

    // Head/tail
    E1000_SENDCMD(E1000_REG_TXDESCHEAD, 0);
    E1000_SENDCMD(E1000_REG_TXDESCTAIL, 0);

    // todo: E1000E?
    uint32_t tctl = E1000_RECVCMD(E1000_REG_TCTRL);

    // Setup collection threshold
    tctl &= ~(0xFF << E1000_TCTL_CT_SHIFT);
    tctl |= (15 << E1000_TCTL_CT_SHIFT);

    // Enable
    tctl |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_RTLC;
    E1000_SENDCMD(E1000_REG_TCTRL, tctl);
    E1000_SENDCMD(0x410, 0x60200a);
}

/**
 * @brief Initialize Rx descriptors
 */
void e1000_rxinit(e1000_t *nic) {
    // First, allocate our Rx descriptors
    nic->rx_descs = (volatile e1000_rx_desc_t*)mem_allocateDMA(sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);
    memset((void*)nic->rx_descs, 0, sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);

    // Now we actually need to allocate our Rx descriptors (i.e. setting up their bits, addresses, etc)
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        // Map in an address
        nic->rx_virt[i] = mem_allocateDMA(PAGE_SIZE); // TODO: Are we sure about this?
        nic->rx_descs[i].addr = mem_getPhysicalAddress(NULL, nic->rx_virt[i]);
        memset((void*)nic->rx_virt[i], 0, PAGE_SIZE);
        nic->rx_descs[i].status = 0;
    } 

    // Write these addresses into the NIC
    uintptr_t phys_rx = mem_getPhysicalAddress(NULL, (uintptr_t)nic->rx_descs);
    E1000_SENDCMD(E1000_REG_RXDESCHI, (uint32_t)((uint64_t)phys_rx >> 32));
    E1000_SENDCMD(E1000_REG_RXDESCLO, (uint32_t)((uint64_t)phys_rx & 0xFFFFFFFF));

    // Setup length of descriptors
    E1000_SENDCMD(E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));

    // Setup head/tail
    E1000_SENDCMD(E1000_REG_RXDESCHEAD, 0);
    E1000_SENDCMD(E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    
    // Setup RCTRL
    E1000_SENDCMD(E1000_REG_RCTRL,
        E1000_RCTL_EN |
        E1000_RCTL_SBP |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE |
        E1000_RCTL_BAM |
        (3 << 16) |
        E1000_RCTL_SECRC |
        (1 << 25));
}


/**
 * @brief Reset controller
 */
void e1000_reset(e1000_t *nic) {
    // Disable IRQs first
	E1000_SENDCMD(E1000_REG_IMC, 0xFFFFFFFF);
	E1000_SENDCMD(E1000_REG_ICR, 0xFFFFFFFF);
	uint32_t status = E1000_RECVCMD(E1000_REG_STATUS);
    (void)status; // Discard

    clock_sleep(100);

    // Turn off Rx and Tx
    E1000_SENDCMD(E1000_REG_RCTRL, 0);
    E1000_SENDCMD(E1000_REG_TCTRL, E1000_TCTL_PSP);
	status = E1000_RECVCMD(E1000_REG_STATUS);
    (void)status; // Discard


    clock_sleep(1000);

    // Reset
    uint32_t ctrl = E1000_RECVCMD(E1000_REG_CTRL);
    ctrl |= E1000_CTRL_RST;
    E1000_SENDCMD(E1000_REG_CTRL, ctrl);
    clock_sleep(500);

    // Disable IRQs again
	E1000_SENDCMD(E1000_REG_IMC, 0xFFFFFFFF);
	E1000_SENDCMD(E1000_REG_ICR, 0xFFFFFFFF);
	status = E1000_RECVCMD(E1000_REG_STATUS);
    (void)status; // Discard

}

/**
 * @brief Set link up on E1000
 */
void e1000_setLinkUp(e1000_t *nic) {
    // Read current CTRL
    uint32_t ctrl = E1000_RECVCMD(E1000_REG_CTRL);
    ctrl |= E1000_CTRL_SLU | (2 << 8); // (2 << 8) for gigabit
    ctrl &= ~(E1000_CTRL_LRST| E1000_CTRL_PHY_RST);
    E1000_SENDCMD(E1000_REG_CTRL, ctrl);

    // Is linked?
    nic->link = ((E1000_RECVCMD(E1000_REG_STATUS) & (1 << 1)));
}

/**
 * @brief Receiver process for E1000
 */
static void e1000_receiverThread(void *data) {
    e1000_t *nic = (e1000_t*)data;

    // If Rx descriptors have been updated then the head will be different
    int head = E1000_RECVCMD(E1000_REG_RXDESCHEAD);

    for (;;) {
        // sleep_untilNever(current_cpu->current_thread);
        // sleep_enter();
        arch_pause();

        if (head == nic->rx_current) {
            // Same as before.. Try reading it one more time
            head = E1000_RECVCMD(E1000_REG_RXDESCHEAD);
        }

        if (head != nic->rx_current) {
            // Sweet, we have packets.
            while (nic->rx_descs[nic->rx_current].status & 0x01) {
                // Let ethernet take care of this
                if (!(nic->rx_descs[nic->rx_current].errors & 0x97)) {
                    // Handle this packet
                    NIC(nic->nic)->stats.rx_dropped++;
                    NIC(nic->nic)->stats.rx_bytes += nic->rx_descs[nic->rx_current].length;
                    ethernet_handle((ethernet_packet_t*)nic->rx_virt[nic->rx_current], nic->nic, nic->rx_descs[nic->rx_current].length);
                } else {
                    LOG(WARN, "Packet has error bits set: 0x%x\n", nic->rx_descs[nic->rx_current].errors);
                    NIC(nic->nic)->stats.rx_dropped++;
                }

                // Reset status
                nic->rx_descs[nic->rx_current].status = 0;

                // rollover
                nic->rx_current++;
                if (nic->rx_current >= E1000_NUM_RX_DESC) {
                    nic->rx_current = 0;
                }

                // Are we at the end?
                if (nic->rx_current == head) {
                    // Try again..
                    head = E1000_RECVCMD(E1000_REG_RXDESCHEAD);
                    if (nic->rx_current == head) {
                        // Yes, break out
                        break;   
                    }
                }

                // Update tail
                E1000_SENDCMD(E1000_REG_RXDESCTAIL, nic->rx_current);

                // Clear STATUS
                uint32_t status = E1000_RECVCMD(E1000_REG_STATUS);
                arch_pause();
            }
        }
    }
}

/**
 * @brief Write method for E1000
 */
static ssize_t e1000_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!buffer || !size || !NIC(node)) return 0;

    // Get E1000
    e1000_t *nic = (e1000_t*)(NIC(node)->driver);
    if (!nic) return 0;

    // Lock the E1000, we're gonna mess with it
    spinlock_acquire(nic->lock);

    // Copy the payload into the Tx buffer
    memcpy((void*)nic->tx_virt[nic->tx_current], buffer, size);

    // Configure Tx descriptor
    nic->tx_descs[nic->tx_current].length = size;
    nic->tx_descs[nic->tx_current].cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS | E1000_CMD_RPS;
    nic->tx_descs[nic->tx_current].status = 0;
    nic->tx_descs[nic->tx_current].css = 0;
    nic->tx_descs[nic->tx_current].special = 0;

    // Increment index
    nic->tx_current++;
    if (nic->tx_current == E1000_NUM_TX_DESC) nic->tx_current = 0; // Reset

    E1000_SENDCMD(E1000_REG_TXDESCTAIL, nic->tx_current);

    // Clear status
    uint32_t status = E1000_RECVCMD(E1000_REG_STATUS);
    (void)status;

    // Update statistics
    NIC(nic->nic)->stats.tx_packets++;
    NIC(nic->nic)->stats.tx_bytes += size;

    // Release
    spinlock_release(nic->lock);
    return size;
}


/**
 * @brief E1000 IRQ handler
 * @param context The NIC
 */
int e1000_irq(void *context) {
    // Get the NIC
    e1000_t *nic = (e1000_t*)context;

    // Does the NIC have anything to say?
    uint32_t icr = E1000_RECVCMD(E1000_REG_ICR);
    if (icr) {
        uint32_t status = E1000_RECVCMD(E1000_REG_STATUS);
        LOG(INFO, "IRQ detected - ICR: %08x STATUS: %08x\n", icr, status); 
        
        if (icr & E1000_ICR_RXT0 || icr & E1000_ICR_RxQ0) {
            // TODO: Seems to cause issues when IRQs are allocated
            // sleep_wakeup(nic->receiver->main_thread);
        } 

        E1000_SENDCMD(E1000_REG_ICR, icr);
    }

    return 0;
}

/**
 * @brief Initialize method for an E1000 device
 * @param dev Device
 * @param type NIC type (device ID)
 */
void e1000_init(pci_device_t *dev, uint16_t type) {
    // First, we should enable PCI I/O space and MMIO space access
    uint16_t cmd = pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_COMMAND_OFFSET, 2);
    cmd |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    cmd &= ~(PCI_COMMAND_INTERRUPT_DISABLE);
    pci_writeConfigOffset(dev->bus, dev->slot, dev->function, PCI_COMMAND_OFFSET, cmd, 2);

    // Allocate an E1000 structure
    e1000_t *nic = kmalloc(sizeof(e1000_t));
    memset(nic, 0, sizeof(e1000_t));
    nic->pci_device = dev;              // PCI
    nic->nic_type = type;               // Device ID
    nic->lock = spinlock_create("e1000 lock");

    // Get the BAR of the device
    pci_bar_t *bar = pci_readBAR(dev->bus, dev->slot, dev->function, 0);
    if (!bar) {
        LOG(WARN, "E1000 device does not have a BAR0.. ok?\n");
        spinlock_destroy(nic->lock);
        kfree(nic);
        return;
    }

    // What type is it?
    if (bar->type == PCI_BAR_IO_SPACE) {
        // TODO
        kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "e1000", "*** No support for I/O space-based E1000 network devices is implemented.\n");
    }

    // Map the MMIO space in
    // Convert if needed
    LOG(DEBUG, "MMIO map: size 0x%016llX addr 0x%016llX bar type %d\n", bar->size, bar->address, bar->type);
    nic->mmio = mem_mapMMIO(bar->address, bar->size);
    kfree(bar);

    // Register our IRQ handler
    uint8_t irq = pci_getInterrupt(dev->bus, dev->slot, dev->function);

    if (irq == 0xFF) {
        LOG(ERR, "E1000 NIC does not have interrupt number\n");
        LOG(ERR, "This is an implementation bug, halting system (REPORT THIS)\n");
        for (;;);
    }

    // Register the interrupt handler
    if (hal_registerInterruptHandler(irq, e1000_irq, (void*)nic)) {
        LOG(ERR, "Error registering IRQ%d for E1000\n", irq);
        
        // Try to enable MSI
        uint8_t vector = pci_enableMSI(dev->bus, dev->slot, dev->function);
        if (vector == 0xFF) {
            LOG(ERR, "MSI configuration failed\n");
            goto _cleanup;
        } else {
            hal_registerInterruptHandler(vector, e1000_irq, (void*)nic);
        }
    }

    // Detect an EEPROM
    nic->eeprom = e1000_detectEEPROM(nic);

    // Read the MAC address
    uint8_t mac[6];
    e1000_readMAC(nic, mac);

    // We have a confirmed NIC, time to create its generic structure.
    nic->nic = nic_create("e1000", mac, NIC_TYPE_ETHERNET, (void*)nic);
    nic->nic->write = e1000_write;

    LOG(INFO, "E1000 found with MAC " MAC_FMT "\n", MAC(mac));

    // Reset the E1000 controller
    e1000_reset(nic);
    LOG(DEBUG, "Reset the NIC successfully\n");

    // Configure flow control (ToaruOS)
    E1000_SENDCMD(0x28, 0x2c8001);
    E1000_SENDCMD(0x2c, 0x100);
    E1000_SENDCMD(0x30, 0x8808);
    E1000_SENDCMD(0x170, 0xFFFF);

    // Link up
    e1000_setLinkUp(nic);
    LOG(DEBUG, "Link up on NIC (status %d)\n", nic->link);

    // Clear the statistical counters
    for (int i = 0; i < 128; i++) E1000_SENDCMD(0x5200 + i * 4, 0);
    

    for (int i = 0; i < 64; i++) {
        uint32_t value = e1000_receiveCommand(nic, 0x4000 + i * 4);
        (void)value; // Discard
    }

    // Okay, let's setup our descriptors
    e1000_rxinit(nic);
    e1000_txinit(nic);

    LOG(DEBUG, "TX/RX descriptors initialized successfully\n");
    LOG(DEBUG, "\tRX descriptors: %p/%p\n", nic->rx_descs, mem_getPhysicalAddress(NULL, (uintptr_t)nic->rx_descs));
    LOG(DEBUG, "\tTX descriptors: %p/%p\n", nic->tx_descs, mem_getPhysicalAddress(NULL, (uintptr_t)nic->tx_descs));

    // RDTR/ITR
    E1000_SENDCMD(E1000_REG_RDTR, 0);
    E1000_SENDCMD(E1000_REG_ITR, 500);
    uint32_t status = e1000_receiveCommand(nic, E1000_REG_STATUS);
    (void)status;

    // Enable IRQs
    E1000_SENDCMD(E1000_REG_IMASK, E1000_ICR_LSC | E1000_ICR_RXO | E1000_ICR_RXT0 | E1000_ICR_TXQE | E1000_ICR_TXDW | E1000_ICR_ACK | E1000_ICR_RXDMT0 | E1000_ICR_SRPD);

    // Set MTU
    NIC(nic->nic)->mtu = 1500;

    // Mount the NIC!
    char name[128];
    snprintf(name, 128, "enp%ds%d", dev->bus, dev->slot);
    nic_register(nic->nic, name);

    nic->receiver = process_createKernel("e1000_receiver", PROCESS_KERNEL, PRIORITY_MED, e1000_receiverThread, (void*)nic);
    scheduler_insertThread(nic->receiver->main_thread);

    // All done
    return;

_cleanup:
    // TODO: Improve this cleanup code. We're probably leaking memory
    if (nic->nic) {
        if (nic->nic->dev) kfree(nic->nic->dev);
        fs_close(nic->nic);
    }

    // Unmap MMIO
    mem_unmapMMIO(nic->mmio, bar->size);
    spinlock_destroy(nic->lock);
    kfree(nic);
}

/**
 * @brief Scan method
 */
int e1000_scan(pci_device_t *dev, void *data) {
    e1000_init(dev, dev->pid);
    return 0;
}

/**
 * @brief Driver initialization method
 */
int driver_init(int argc, char **argv) {
    pci_id_mapping_t id_map[] = {
        { .vid = VENDOR_ID_INTEL, .pid = { E1000_DEVICE_EMU, E1000_DEVICE_I217, E1000_DEVICE_82577LM, E1000_DEVICE_82574L, E1000_DEVICE_82545EM, E1000_DEVICE_82543GC, PCI_NONE }},
        PCI_ID_MAPPING_END
    };

    pci_scan_parameters_t params = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = id_map,
    };

    pci_scanDevice(e1000_scan, &params, NULL);
    return 0;
}

/**
 * @brief Driver deinitialization method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "E1000 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};

