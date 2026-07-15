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
#include <kernel/drivers/net/ethernet.h>
#include <kernel/drivers/clock.h>
#include <kernel/loader/driver.h>
#include <kernel/task/process.h>
#include <kernel/drivers/pci.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
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

/* NIC */
static ssize_t e1000_send(nic_t *nic, size_t size, char *buffer);
static nic_ops_t e1000_nic_ops = {
    .send = e1000_send,
    .ioctl = NULL
};

/**
 * @brief Send a command to the nic
 * @param nic The nic to send to
 * @param reg The register to send the command to
 * @param value The value to write
 */
static inline void e1000_sendCommand(e1000_t *nic, uint16_t reg, uint32_t value) {
    E1000_WRITE32(reg, value);
}

/**
 * @brief Receive a command from the nic
 * @param nic The nic to get from
 * @param reg The register to read from
 */
static inline uint32_t e1000_receiveCommand(e1000_t *nic, uint16_t reg) {
    uint32_t value = E1000_READ32(reg);
    return value;
}

/**
 * @brief EEPROM detection
 * @param nic NIC
 */
bool e1000_detectEEPROM(e1000_t *nic) {
    // TODO: Certain models do not support EEPROM, don't waste time.
    E1000_SENDCMD(E1000_REG_EEPROM, 1); // EEPROM reads a word at EE_ADDR and stores in EE_DATA

    // Wait for the DONE bit to be set
    for (int i = 0; i < 2000; i++) {
        uint32_t eeprom = E1000_RECVCMD(E1000_REG_EEPROM);
        if (eeprom & 0x10) return true;
    }

    return false;
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
    nic->tx_descs = (volatile e1000_tx_desc_t*)dma_map(sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);
    memset((void*)nic->tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);

    // Now we actually need to allocate our Tx descriptors (i.e. setting up their bits, addresses, etc)
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        // Map in an address
        nic->tx_virt[i] = dma_map(PAGE_SIZE*2); // TODO: Are we sure about this?
        nic->tx_descs[i].addr = arch_mmu_physical(NULL, nic->tx_virt[i]);

        // Mark as EOP
        nic->tx_descs[i].status = 0xff;
        nic->tx_descs[i].cmd = E1000_CMD_EOP;
    } 

    // Write these addresses into the NIC
    uintptr_t phys_tx = arch_mmu_physical(NULL, (uintptr_t)nic->tx_descs);
    E1000_SENDCMD(E1000_REG_TXDESCHI, (uint32_t)((uint64_t)phys_tx >> 32));
    E1000_SENDCMD(E1000_REG_TXDESCLO, (uint32_t)((uint64_t)phys_tx & 0xFFFFFFFF));

    // Setup length of descriptors
    E1000_SENDCMD(E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));

    // Head/tail
    E1000_SENDCMD(E1000_REG_TXDESCHEAD, 0);
    E1000_SENDCMD(E1000_REG_TXDESCTAIL, 0);

    // Intel I219: Enable Tx queue
    E1000_SENDCMD(E1000_REG_TXDCTL, E1000_RECVCMD(E1000_REG_TXDCTL) | (1 << 25));
    while (!(E1000_RECVCMD(E1000_REG_TXDCTL) & (1 << 25))) {
        LOG(DEBUG, "waiting for TXDCTL to enable (0x%x)\n", E1000_RECVCMD(E1000_REG_TXDCTL));
        arch_pause(); 
    }

    // Enable
    E1000_SENDCMD(E1000_REG_TCTRL, E1000_TCTL_EN | E1000_TCTL_PSP);
    E1000_SENDCMD(0x410, 0x60200a);
}

/**
 * @brief Initialize Rx descriptors
 */
void e1000_rxinit(e1000_t *nic) {
    // First, allocate our Rx descriptors
    nic->rx_descs = (volatile e1000_rx_desc_t*)dma_map(sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);
    memset((void*)nic->rx_descs, 0, sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);

    // Now we actually need to allocate our Rx descriptors (i.e. setting up their bits, addresses, etc)
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        // Map in an address
        nic->rx_virt[i] = dma_map(PAGE_SIZE*2); // TODO: Are we sure about this?
        nic->rx_descs[i].addr = arch_mmu_physical(NULL, nic->rx_virt[i]);
        nic->rx_descs[i].status = 0;
    } 

    // Write these addresses into the NIC
    uintptr_t phys_rx = arch_mmu_physical(NULL, (uintptr_t)nic->rx_descs);
    E1000_SENDCMD(E1000_REG_RXDESCHI, (uint32_t)((uint64_t)phys_rx >> 32));
    E1000_SENDCMD(E1000_REG_RXDESCLO, (uint32_t)((uint64_t)phys_rx & 0xFFFFFFFF));

    // Setup length of descriptors
    E1000_SENDCMD(E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));

    // Setup head/tail
    E1000_SENDCMD(E1000_REG_RXDESCHEAD, 0);
    E1000_SENDCMD(E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    
    // Intel I219: Enable Rx queue
    E1000_SENDCMD(E1000_REG_RXDCTL, E1000_RECVCMD(E1000_REG_RXDCTL) | (1 << 25));
    while (!(E1000_RECVCMD(E1000_REG_RXDCTL) & (1 << 25))) {
        arch_pause(); 
    }

    // Setup RCTRL
    E1000_SENDCMD(E1000_REG_RCTRL,
        E1000_RCTL_EN |
        E1000_RCTL_SBP |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE |
        E1000_RCTL_BAM |
        (3 << 16) |
        E1000_RCTL_SECRC |
        (1 << 25) |
        (3 << 16));
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


    clock_sleep(100);

    // Reset
    uint32_t ctrl = E1000_RECVCMD(E1000_REG_CTRL);
    ctrl |= E1000_CTRL_RST;
    E1000_SENDCMD(E1000_REG_CTRL, ctrl);
    clock_sleep(100);

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
}

/**
 * @brief Receiver process for E1000
 */
static void e1000_receiverThread(void *data) {
    e1000_t *nic = (e1000_t*)data;

    for (;;) {
        spinlock_acquire(&nic->rcv_lck);

        int head = E1000_RECVCMD(E1000_REG_RXDESCHEAD);
        int packets_processed = 0;

        while (nic->rx_current != head) {
            volatile e1000_rx_desc_t *d = &nic->rx_descs[nic->rx_current];

            // give the NIC a bit of time to process whats happening
            while (!(d->status & 0x01)) {
                asm volatile ("pause" ::: "memory");
            }

            if ((d->errors & 0x97) == 0) {
                nic->n->stats.rx_packets++;
                nic->n->stats.rx_bytes += d->length;
                spinlock_release(&nic->rcv_lck);
                ethernet_handle((ethernet_packet_t*)nic->rx_virt[nic->rx_current], nic->n, d->length);
                spinlock_acquire(&nic->rcv_lck);
            } else {
                nic->n->stats.rx_dropped++;
            }

            d->status = 0;
            packets_processed++;

            nic->rx_current++;
            if (nic->rx_current >= E1000_NUM_RX_DESC) {
                nic->rx_current = 0;
            }

            head = E1000_RECVCMD(E1000_REG_RXDESCHEAD);
        }

        if (packets_processed > 0) {
            int next_rx_tail = (nic->rx_current) ? nic->rx_current - 1 : E1000_NUM_RX_DESC - 1;
            E1000_SENDCMD(E1000_REG_RXDESCTAIL, next_rx_tail);
        }

        sleep_prepare();
        spinlock_release(&nic->rcv_lck);
        sleep_enter();
    }
}

/**
 * @brief Write method for E1000
 */
static ssize_t e1000_send(nic_t *n, size_t size, char *buffer) {
    // Get E1000
    e1000_t *nic = n->driver;

    int tx_current = __atomic_fetch_add(&nic->tx_current, 1, __ATOMIC_SEQ_CST) % E1000_NUM_TX_DESC;

    volatile e1000_tx_desc_t *d = (e1000_tx_desc_t*)(&nic->tx_descs[tx_current]);
    while (d->status == 0) {
        process_yield(1);
    }

    void *tx_buffer = (void*)nic->tx_virt[tx_current];
    memcpy(tx_buffer, buffer, size);

    d->length = size;
    d->status = 0;
    d->cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS;

    if (tx_current == __atomic_fetch_add(&nic->tx_current2, 1, __ATOMIC_SEQ_CST) % E1000_NUM_TX_DESC) {
        E1000_SENDCMD(E1000_REG_TXDESCTAIL, (tx_current+1) % E1000_NUM_TX_DESC);
    }
    return size;
}


/**
 * @brief E1000 tasklet
 */
void e1000_tasklet(void *context) {
    e1000_t *nic = (e1000_t*)context;

    uint32_t icr = E1000_RECVCMD(E1000_REG_ICR);
    if (icr) {
        if (icr & E1000_ICR_LSC) {
            nic->n->state = ((E1000_RECVCMD(E1000_REG_STATUS) & (1 << 1))) ? NIC_STATE_UP : NIC_STATE_DOWN;
        }
        
        if (icr & E1000_ICR_RXT0 || icr & E1000_ICR_RxQ0) {
            spinlock_acquire(&nic->rcv_lck);
            sleep_wakeup(nic->receiver->main_thread);
            spinlock_release(&nic->rcv_lck);
        } 

        if (icr & (E1000_ICR_RXO | E1000_ICR_RXSEQ)) {
            LOG(ERR, "RX error (0x%x)\n", icr);
        }
    }
}

tasklet_t tsk;

/**
 * @brief E1000 IRQ handler
 * @param context The NIC
 */
int e1000_irq(irq_t *irq, void *context) {
    // Get the NIC
    e1000_t *nic = (e1000_t*)context;

    tasklet_insert(&tsk);

    return IRQ_HANDLED;
}

/**
 * @brief Initialize method for an E1000 device
 * @param dev Device
 * @param type NIC type (device ID)
 * @param is_e1000e Is E1000E
 */
void e1000_init(pci_device_t *dev, uint16_t type, bool is_e1000e) {
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
    SPINLOCK_INIT(&nic->rcv_lck);

    // Get the BAR of the device
    pci_bar_t *bar = pci_readBAR(dev->bus, dev->slot, dev->function, 0);
    if (!bar) {
        LOG(WARN, "E1000 device does not have a BAR0.. ok?\n");
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
    nic->mmio = mmio_map(bar->address, bar->size);
    kfree(bar);

    // Allocate interrupts
    int r = pci_allocateInterrupts(dev, 1, 1, PCI_IRQ_ALL);
    if (r != 1) {
        LOG(ERR, "E1000 NIC failed to allocate interrupts.\n");
        goto _cleanup;
    }

    // Register the IRQ
    pci_irq_t *irq = pci_getInterruptVector(dev, 0);
    irq_register(irq->vector, e1000_irq, IRQ_FLAG_SHARED, (void*)nic, NULL);

    // Detect an EEPROM
    if (is_e1000e == false) {
        nic->eeprom = e1000_detectEEPROM(nic);
    } else {
        nic->eeprom = false;
    }

    // Read the MAC address
    uint8_t mac[6];
    e1000_readMAC(nic, mac);

    // We have a confirmed NIC, time to create its generic structure.
    char name[128];
    snprintf(name, 128, "enp%ds%d", dev->bus, dev->slot);

    nic->n = nic_create(name, NIC_TYPE_ETHERNET, &e1000_nic_ops, mac, (void*)nic);
    LOG(INFO, "E1000 found with MAC " MAC_FMT "\n", MAC(mac));

    // Reset the E1000 controller
    e1000_reset(nic);
    LOG(DEBUG, "Reset the NIC successfully\n");

    // Link up
    e1000_setLinkUp(nic);
    nic->n->state = ((E1000_RECVCMD(E1000_REG_STATUS) & (1 << 1))) ? NIC_STATE_UP : NIC_STATE_DOWN;
    LOG(DEBUG, "Link is %s\n", nic->n->state == NIC_STATE_UP ? "UP" : "DOWN");

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
    LOG(DEBUG, "\tRX descriptors: %p/%p\n", nic->rx_descs, arch_mmu_physical(NULL, (uintptr_t)nic->rx_descs));
    LOG(DEBUG, "\tTX descriptors: %p/%p\n", nic->tx_descs, arch_mmu_physical(NULL, (uintptr_t)nic->tx_descs));

    // RDTR/ITR
    E1000_SENDCMD(E1000_REG_RDTR, 0);
    E1000_SENDCMD(E1000_REG_ITR, 500);
    uint32_t status = e1000_receiveCommand(nic, E1000_REG_STATUS);
    (void)status;

    // Enable IRQs
    TASKLET_INIT(&tsk, "e1000", e1000_tasklet, nic);
    E1000_SENDCMD(E1000_REG_ICR, 0xFFFFFFFF);
    E1000_SENDCMD(E1000_REG_IMASK, 0xFFFFFFFF);

    // Set MTU
    nic->n->mtu = 1500;

    nic->receiver = process_createKernel("e1000_receiver", PROCESS_KERNEL, PRIORITY_MED, e1000_receiverThread, (void*)nic);
    scheduler_insertThread(nic->receiver->main_thread);

    // All done
    return;

_cleanup:
    // TODO: Improve this cleanup code. We're probably leaking memory

    // Unmap MMIO
    mmio_unmap(nic->mmio, bar->size);
    kfree(nic);
}

/**
 * @brief Scan method
 */
int e1000_scan(pci_device_t *dev, void *data) {
    e1000_init(dev, dev->devid, false);
    return 0;
}


/**
 * @brief Scan method (E1000E-series)
 */
int e1000e_scan(pci_device_t *dev, void *data) {
    e1000_init(dev, dev->devid, true);
    return 0;
}


/**
 * @brief Driver initialization method
 */
int driver_init(int argc, char **argv) {
    pci_id_mapping_t e1000_id_map[] = {
        { .vid = VENDOR_ID_INTEL, .devid = {
            E1000_DEVICE_EMU,
            E1000_DEVICE_82574L,
            E1000_DEVICE_82545EM,
            E1000_DEVICE_82543GC,
            PCI_NONE }},
        PCI_ID_MAPPING_END
    };

    pci_scan_parameters_t e1000_params = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = e1000_id_map,
    };

    pci_scanDevice(e1000_scan, &e1000_params, NULL);

    pci_id_mapping_t e1000e_id_map[] = {
        { .vid = VENDOR_ID_INTEL, .devid = {
            E1000_DEVICE_82577LM,
            E1000_DEVICE_I217,
            0x15d8,
            PCI_NONE }},
        PCI_ID_MAPPING_END
    };

    pci_scan_parameters_t e1000e_params = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = e1000e_id_map,
    };

    pci_scanDevice(e1000e_scan, &e1000e_params, NULL);

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

