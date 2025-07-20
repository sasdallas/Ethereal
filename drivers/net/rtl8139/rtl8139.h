/**
 * @file drivers/net/rtl8139/rtl8139.h
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

#ifndef _RTL8139_H
#define _RTL8139_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/task/process.h>
#include <kernel/drivers/pci.h>
#include <kernel/misc/spinlock.h>

/**** DEFINITIONS ****/

/* Registers */
#define RTL8139_REG_MAC             0x00    // MAC address (4 registers)
#define RTL8139_REG_MAR             0x08    // Multicast filter
#define RTL8139_REG_TSD             0x10    // Tx status (4 registers)
#define RTL8139_REG_TSAD            0x20    // Tx address (4 registers)
#define RTL8139_REG_RBSTART         0x30    // Rx buffer start
#define RTL8139_REG_ERBCR           0x34    // Early receive (Rx) byte count register
#define RTL8139_REG_ERSR            0x36    // Early Rx status register
#define RTL8139_REG_CR              0x37    // Command register
#define RTL8139_REG_CAPR            0x38    // (C) Current address of packet read
#define RTL8139_REG_CBR             0x3A    // (C) Current buffer address
#define RTL8139_REG_IMR             0x3C    // Interrupt mask register
#define RTL8139_REG_ISR             0x3E    // Interrupt status register
#define RTL8139_REG_TCR             0x40    // Tx configuration
#define RTL8139_REG_RCR             0x44    // Rx configuration
#define RTL8139_REG_TCTR            0x48    // Timer count register

/* Commands register */
#define RTL8139_CMD_BUFE            (1 << 0)    // Buffer empty
#define RTL8139_CMD_TE              (1 << 2)    // Transmitter enable
#define RTL8139_CMD_RE              (1 << 3)    // Receiver enable
#define RTL8139_CMD_RST             (1 << 4)    // Reset

/* IMR */
#define RTL8139_IMR_ROK             (1 << 0)    // Rx ok
#define RTL8139_IMR_RER             (1 << 1)    // Rx error
#define RTL8139_IMR_TOK             (1 << 2)    // Tx ok
#define RTL8139_IMR_TER             (1 << 3)    // Tx error
#define RTL8139_IMR_RXOVER          (1 << 4)    // Rx overflow
#define RTL8139_IMR_RXUNDER         (1 << 5)    // Rx underrun
#define RTL8139_IMR_RXFIFO          (1 << 6)    // Rx FIFO oevrflow
#define RTL8139_IMR_TIMEOUT         (1 << 14)   // Timeout
#define RTL8139_IMR_SERR            (1 << 15)   // System error

/* ISR */
#define RTL8139_ISR_ROK             (1 << 0)    // Rx ok
#define RTL8139_ISR_RER             (1 << 1)    // Rx error
#define RTL8139_ISR_TOK             (1 << 2)    // Tx ok
#define RTL8139_ISR_TER             (1 << 3)    // Tx error
#define RTL8139_ISR_RXOVER          (1 << 4)    // Rx overflow
#define RTL8139_ISR_RXUNDER         (1 << 5)    // Rx underrun
#define RTL8139_ISR_RXFIFO          (1 << 6)    // Rx FIFO oevrflow
#define RTL8139_ISR_TIMEOUT         (1 << 14)   // Timeout
#define RTL8139_ISR_SERR            (1 << 15)   // System error

/* Tx status */
#define RTL8139_TSD_OWN             (1 << 13)   // Operation completed
#define RTL8139_TSD_TUN             (1 << 14)   // Transmit FIFO underrun
#define RTL8139_TSD_TOK             (1 << 15)   // Transmit OK
#define RTL8139_TSD_ERTXTH_SHIFT    16
#define RTL8139_TSD_NCC_SHIFT       24
#define RTL8139_TSD_CDH             (1 << 28)   // CD heart beat
#define RTL8139_TSD_OWC             (1 << 29)   // Out of Window
#define RTL8139_TSD_TABT            (1 << 30)   // Transfer abort
#define RTL8139_TSD_CRS             (1 << 31)   // Carrier sense lost

/* Rx status */
#define RTL8139_RSR_ROK             (1 << 0)    // Receive OK
#define RTL8139_RSR_FAE             (1 << 1)    // Frame alignment error
#define RTL8139_RSR_CRC             (1 << 2)    // CRC error
#define RTL8139_RSR_LONG            (1 << 3)    // Long packet
#define RTL8139_RSR_RUNT            (1 << 4)    // Tiny, insignificant loser short packet detected
#define RTL8139_RSR_ISE             (1 << 5)    // Invalid symbol error
#define RTL8139_RSR_BAR             (1 << 13)   // Broadcast address received
#define RTL8139_RSR_PAM             (1 << 14)   // Physical address matches
#define RTL8139_RSR_MAR             (1 << 15)   // Multicast address received

/* Rx configuration */
#define RTL8139_RCR_AAP             (1 << 0)    // Accept physical address packets
#define RTL8139_RCR_APM             (1 << 1)    // Accept physical match packets
#define RTL8139_RCR_AM              (1 << 2)    // Accept multicast packets
#define RTL8139_RCR_AR              (1 << 3)    // Accept runt packets
#define RTL8139_RCR_AER             (1 << 4)    // Accept error packets
#define RTL8139_RCR_WRAP            (1 << 7)    // Wrap

/**** TYPES ****/

typedef struct rtl8139 {
    pci_device_t *pci_device;   // PCI device
    uintptr_t mmio_addr;        // MMIO/IO space address
    uint8_t io_space;           // I/O space toggle
    spinlock_t lock;            // Garbage lock

    uintptr_t rx_buffer;        // Rx buffer
    process_t *receive_proc;    // Receiver process
    uint32_t rx_current;        // Current Rx indicator

    uintptr_t tx_buffer;        // The one, single Tx buffer that we allow. Why?
    size_t tx_buffer_size;      // Tx buffer size
    uint32_t tx_current;        // Current Tx indicator

    fs_node_t *nic;             // NIC object
} rtl8139_t;

#endif