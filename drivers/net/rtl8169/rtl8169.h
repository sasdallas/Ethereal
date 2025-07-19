/**
 * @file drivers/net/rtl8169/rtl8169.h
 * @brief RTL8169 network card driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _RTL8169_H
#define _RTL8169_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/net/nic.h>
#include <kernel/task/process.h>

/**** DEFINITIONS ****/

/* Registers */
#define RTL8169_REG_IDR0            0x00
#define RTL8169_REG_IDR1            0x01
#define RTL8169_REG_IDR2            0x02
#define RTL8169_REG_IDR3            0x03
#define RTL8169_REG_IDR4            0x04
#define RTL8169_REG_IDR5            0x05  
#define RTL8169_REG_TNPDS           0x20            // Transmit Normal Priority Descriptors
#define RTL8169_REG_CR              0x37            // Command register
#define RTL8169_REG_TPPoll          0x38            // TPPoll
#define RTL8169_REG_IMR             0x3C            // Interrupt mask register
#define RTL8169_REG_ISR             0x3E            // Interrupt status register
#define RTL8169_REG_TCR             0x40            // Transmit control register
#define RTL8169_REG_RCR             0x44            // Receive control register
#define RTL8169_REG_TCTR            0x48            // Timer count register
#define RTL8169_REG_MPC             0x4C            // Missed packet counter (Rx FIFO)
#define RTL8169_REG_9346CR          0x50            // 93C46 command register
#define RTL8169_REG_PHYStatus       0x6C            // PHY (GMII, MII, or TBI) status
#define RTL8169_REG_RMS             0xDA            // Rx packet maximum size
#define RTL8169_REG_RDSAR           0xE4            // Receive descriptor start address
#define RTL8169_REG_MTPS            0xEC            // Max transmit packet size register

/* CR */
#define RTL8169_CR_TE               (1 << 2)        // Transmit enable
#define RTL8169_CR_RE               (1 << 3)        // Receive enable
#define RTL8169_CR_RST              (1 << 4)        // Reset

/* TPPoll */
#define RTL8169_TPPoll_FSWInt       (1 << 0)        // Forced software int
#define RTL8169_TPPoll_NPQ          (1 << 6)        // Normal priority queue polling
#define RTL8169_TPPoll_HPQ          (1 << 7)        // High priority queue polling

/* IMR */
#define RTL8169_IMR_ROK             (1 << 0)        // Rx OK
#define RTL8169_IMR_RER             (1 << 1)        // Rx ERROR
#define RTL8169_IMR_TOK             (1 << 2)        // Tx OK
#define RTL8169_IMR_TER             (1 << 3)        // Tx ERROR
#define RTL8169_IMR_RDU             (1 << 4)        // Rx descriptor unavailable
#define RTL8169_IMR_LINKCHG         (1 << 5)        // Link change
#define RTL8169_IMR_FOVW            (1 << 6)        // Rx FIFO overflow
#define RTL8169_IMR_TDU             (1 << 7)        // Tx descriptor unavailable
#define RTL8169_IMR_SWINT           (1 << 8)        // Software interrupt
#define RTL8169_IMR_TIMEOUT         (1 << 14)       // Timeout interrupt

/* ISR */
#define RTL8169_ISR_ROK             (1 << 0)        // Rx OK
#define RTL8169_ISR_RER             (1 << 1)        // Rx ERROR
#define RTL8169_ISR_TOK             (1 << 2)        // Tx OK
#define RTL8169_ISR_TER             (1 << 3)        // Tx ERROR
#define RTL8169_ISR_RDU             (1 << 4)        // Rx descriptor unavailable
#define RTL8169_ISR_LINKCHG         (1 << 5)        // Link change
#define RTL8169_ISR_FOVW            (1 << 6)        // Rx FIFO overflow
#define RTL8169_ISR_TDU             (1 << 7)        // Tx descriptor unavailable
#define RTL8169_ISR_SWINT           (1 << 8)        // Software interrupt
#define RTL8169_ISR_TIMEOUT         (1 << 14)       // Timeout interrupt
#define RTL8169_ISR_SERR            (1 << 15)       // System error

/* MXDMA sizes for TCR */
#define RTL8169_TCR_MXDMA_SHIFT     8
#define RTL8169_TCR_MXDMA16         (0x00 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA32         (0x01 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA64         (0x02 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA128        (0x03 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA256        (0x04 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA512        (0x05 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA1024       (0x06 << RTL8169_TCR_MXDMA_SHIFT)
#define RTL8169_TCR_MXDMA_UNLIMITED (0x07 << RTL8169_TCR_MXDMA_SHIFT)

/* TCR */
#define RTL8169_TCR_NOCRC           (1 << 16)
#define RTL8169_TCR_LBK_MAC         (1 << 17)

/* MXDMA sizes for RCR */
#define RTL8169_RCR_MXDMA_SHIFT     8
#define RTL8169_RCR_MXDMA16         (0x00 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA32         (0x01 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA64         (0x02 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA128        (0x03 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA256        (0x04 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA512        (0x05 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA1024       (0x06 << RTL8169_RCR_MXDMA_SHIFT)
#define RTL8169_RCR_MXDMA_UNLIMITED (0x07 << RTL8169_RCR_MXDMA_SHIFT)

/* RXFTH sizes for RCR */
#define RTL8169_RCR_RXFTH_SHIFT     13
#define RTL8169_RCR_RXFTH64         (0x02 << RTL8169_RCR_RXFTH_SHIFT)
#define RTL8169_RCR_RXFTH128        (0x03 << RTL8169_RCR_RXFTH_SHIFT)
#define RTL8169_RCR_RXFTH256        (0x04 << RTL8169_RCR_RXFTH_SHIFT)
#define RTL8169_RCR_RXFTH512        (0x05 << RTL8169_RCR_RXFTH_SHIFT)
#define RTL8169_RCR_RXFTH1024       (0x06 << RTL8169_RCR_RXFTH_SHIFT)
#define RTL8169_RCR_RXFTH_UNLIMITED (0x07 << RTL8169_RCR_RXFTH_SHIFT)


/* RCR */
#define RTL8169_RCR_AAP             (1 << 0)        // Accept all packets with destination
#define RTL8169_RCR_APM             (1 << 1)        // Accept physical matches
#define RTL8169_RCR_AM              (1 << 2)        // Accept multicast
#define RTL8169_RCR_AB              (1 << 3)        // Accept broadcast
#define RTL8169_RCR_AR              (1 << 4)        // Accept runt
#define RTL8169_RCR_AER             (1 << 5)        // Acept error
#define RTL8169_RCR_9356SEL         (1 << 6)        // 9346/9356 EEPROM select

/* PHYStatus */
#define RTL8169_PHYStatus_FULLDUP   (1 << 0)        // Full-Duplex status
#define RTL8169_PHYStatus_LINKSTS   (1 << 1)        // Link Ok
#define RTL8169_PHYStatus_10M       (1 << 2)        // 10Mbps link speed
#define RTL8169_PHYStatus_100M      (1 << 3)        // 100Mbps link speed
#define RTL8169_PHYStatus_1000MF    (1 << 4)        // 1000Mbps full-duplex link speed
#define RTL8169_PHYStatus_RXFLOW    (1 << 5)        // Receive flow control
#define RTL8169_PHYStatus_TXFLOW    (1 << 6)        // Transmit flow control

/* Desc command value */
#define RTL8169_DESC_CMD_LGSEN      (1 << 27)       // Large send
#define RTL8169_DESC_CMD_LS         (1 << 28)       // Last segment descriptor
#define RTL8169_DESC_CMD_FS         (1 << 29)       // First segment descriptor
#define RTL8169_DESC_CMD_EOR        (1 << 30)       // End of descriptor
#define RTL8169_DESC_CMD_OWN        (1 << 31)       // Ownership

/* 9346 */
#define RTL8169_9346CR_MODE_CONFIG  (0x3 << 6)

/* Counts */
#define RTL8169_RX_DESC_COUNT       256
#define RTL8169_TX_DESC_COUNT       256
#define RTL8169_RX_BUFFER_SIZE      8192
#define RTL8169_TX_BUFFER_SIZE      8192


/**** TYPES ****/

typedef struct rtl8169_desc {
    uint32_t command;           // Command
    uint32_t vlan;              // VLAN
    uint32_t buffer_lo;         // Low bits of buffer
    uint32_t buffer_hi;         // High bits of buffer
} __attribute__((packed)) rtl8169_desc_t;

typedef struct rtl8169 {
    fs_node_t *nic;             // NIC
    uintptr_t base;             // I/O base address

    process_t *recv_proc;       // Receive process
    struct thread *thr;         // Waiting thread

    spinlock_t lock;            // Lock
    
    uintptr_t tx_buffers;       // Tx buffer region
    uintptr_t tx_descriptors;   // Tx descriptor regions
    uint32_t tx_current;        // Current Tx index

    uintptr_t rx_buffers;       // Rx buffer region
    uintptr_t rx_descriptors;   // Rx descriptor regions
    uint32_t rx_current;        // Current Rx index
} rtl8169_t;

#endif