/**
 * @file drivers/net/e1000/e1000.h
 * @brief E1000 driver
 * 
 * @ref https://wiki.osdev.org/Intel_Ethernet_i217
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef E1000_H
#define E1000_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/spinlock.h>
#include <kernel/drivers/pci.h>

/**** DEFINITIONS ****/

/* Vendor and device IDs */
#define VENDOR_ID_INTEL         0x8086      // Intel vendor ID
#define E1000_DEVICE_EMU        0x100E      // Emulator NIC device ID (QEMU, Bochs, VirtualBox)
#define E1000_DEVICE_I217       0x153A      // Intel I217
#define E1000_DEVICE_82577LM    0x10EA      // Intel 82577LM
#define E1000_DEVICE_82574L     0x10D3      // Intel 82574L
#define E1000_DEVICE_82545EM    0x100F      // Intel 82545EM
#define E1000_DEVICE_82543GC    0x1004      // Intel 82543GC

/* Registers */
#define E1000_REG_CTRL              0x0000
#define E1000_REG_STATUS            0x0008
#define E1000_REG_EECD              0x0010
#define E1000_REG_EEPROM            0x0014
#define E1000_REG_CTRL_EXT          0x0018
#define E1000_REG_ICR               0x00C0
#define E1000_REG_ITR               0x00C4
#define E1000_REG_IMASK             0x00D0
#define E1000_REG_IMC               0x00D8
#define E1000_REG_RCTRL             0x0100
#define E1000_REG_RXDESCLO          0x2800
#define E1000_REG_RXDESCHI          0x2804
#define E1000_REG_RXDESCLEN         0x2808
#define E1000_REG_RXDESCHEAD        0x2810
#define E1000_REG_RXDESCTAIL        0x2818
#define E1000_REG_RDTR              0x2820

#define E1000_REG_TCTRL             0x0400
#define E1000_REG_TXDESCLO          0x3800
#define E1000_REG_TXDESCHI          0x3804
#define E1000_REG_TXDESCLEN         0x3808
#define E1000_REG_TXDESCHEAD        0x3810
#define E1000_REG_TXDESCTAIL        0x3818

#define E1000_REG_RXADDR            0x5400
#define E1000_REG_RXADDRHIGH        0x5404

/* EECD */
#define E1000_EECD_EE_REQ                       (1 << 6)    // Request EEPROM access
#define E1000_EECD_EE_GNT                       (1 << 7)    // Grant EEPROM acesss
#define E1000_EECD_EE_PRES                      (1 << 8)    // EEPROM present
#define E1000_EECD_EE_SIZE                      (1 << 9)    // 1024/4096 size

/* CTRL */
#define E1000_CTRL_PHY_RST                      (1 << 31)
#define E1000_CTRL_RST                          (1 << 26)
#define E1000_CTRL_SLU                          (1 << 6)
#define E1000_CTRL_LRST                         (1 << 3)

/* RCTRL bits */
#define E1000_RCTL_EN                           (1 << 1)    // Receiver Enable
#define E1000_RCTL_SBP                          (1 << 2)    // Store Bad Packets
#define E1000_RCTL_UPE                          (1 << 3)    // Unicast Promiscuous Enabled
#define E1000_RCTL_MPE                          (1 << 4)    // Multicast Promiscuous Enabled
#define E1000_RCTL_LPE                          (1 << 5)    // Long Packet Reception Enable
#define E1000_RCTL_LBM_NONE                     (0 << 6)    // No Loopback
#define E1000_RCTL_LBM_PHY                      (3 << 6)    // PHY or external SerDesc loopback
#define E1000_RCTL_RDMTS_HALF                   (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define E1000_RCTL_RDMTS_QUARTER                (1 << 8)    // Free Buffer Threshold is 1/4 of RDLEN
#define E1000_RCTL_RDMTS_EIGHTH                 (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define E1000_RCTL_MO_36                        (0 << 12)   // Multicast Offset - bits 47:36
#define E1000_RCTL_MO_35                        (1 << 12)   // Multicast Offset - bits 46:35
#define E1000_RCTL_MO_34                        (2 << 12)   // Multicast Offset - bits 45:34
#define E1000_RCTL_MO_32                        (3 << 12)   // Multicast Offset - bits 43:32
#define E1000_RCTL_BAM                          (1 << 15)   // Broadcast Accept Mode
#define E1000_RCTL_VFE                          (1 << 18)   // VLAN Filter Enable
#define E1000_RCTL_CFIEN                        (1 << 19)   // Canonical Form Indicator Enable
#define E1000_RCTL_CFI                          (1 << 20)   // Canonical Form Indicator Bit Value
#define E1000_RCTL_DPF                          (1 << 22)   // Discard Pause Frames
#define E1000_RCTL_PMCF                         (1 << 23)   // Pass MAC Control Frames
#define E1000_RCTL_SECRC                        (1 << 26)   // Strip Ethernet CRC

/* RCTRL buffer sizes */
#define E1000_RCTL_BUFSZ_256                    (3 << 16)
#define E1000_RCTL_BUFSZ_512                    (2 << 16)
#define E1000_RCTL_BUFSZ_1024                   (1 << 16)
#define E1000_RCTL_BUFSZ_2048                   (0 << 16)
#define E1000_RCTL_BUFSZ_4096                   ((3 << 16) | (1 << 25))
#define E1000_RCTL_BUFSZ_8192                   ((2 << 16) | (1 << 25))
#define E1000_RCTL_BUFSZ_16384                  ((1 << 16) | (1 << 25))

/* TCTRL bits */
#define E1000_TCTL_EN                           (1 << 1)    // Transmit Enable
#define E1000_TCTL_PSP                          (1 << 3)    // Pad Short Packets
#define E1000_TCTL_CT_SHIFT                     4           // Collision Threshold
#define E1000_TCTL_COLD_SHIFT                   12          // Collision Distance
#define E1000_TCTL_SWXOFF                       (1 << 22)   // Software XOFF Transmission
#define E1000_TCTL_RTLC                         (1 << 24)   // Re-transmit on Late Collision

/* TSTA */
#define E1000_TSTA_DD                           (1 << 0)    // Descriptor Done
#define E1000_TSTA_EC                           (1 << 1)    // Excess Collisions
#define E1000_TSTA_LC                           (1 << 2)    // Late Collision

/* Commands */
#define E1000_CMD_EOP                           (1 << 0)    // End of Packet
#define E1000_CMD_IFCS                          (1 << 1)    // Insert FCS
#define E1000_CMD_IC                            (1 << 2)    // Insert Checksum
#define E1000_CMD_RS                            (1 << 3)    // Report Status
#define E1000_CMD_RPS                           (1 << 4)    // Report Packet Sent
#define E1000_CMD_VLE                           (1 << 6)    // VLAN Packet Enable
#define E1000_CMD_IDE                           (1 << 7)    // Interrupt Delay Enable

/* Descriptor numbers */
#define E1000_NUM_TX_DESC                       512
#define E1000_NUM_RX_DESC                       512

/* ICR */
#define E1000_ICR_TXDW                          (1 << 0)
#define E1000_ICR_TXQE                          (1 << 1)  // Transmit queue is empty
#define E1000_ICR_LSC                           (1 << 2)  // Link status changed
#define E1000_ICR_RXSEQ                         (1 << 3)  // Receive sequence count error
#define E1000_ICR_RXDMT0                        (1 << 4)  // Receive descriptor minimum threshold
#define E1000_ICR_RXO                           (1 << 6)  // Receive overrun
#define E1000_ICR_RXT0                          (1 << 7) 
#define E1000_ICR_ACK                           (1 << 17)
#define E1000_ICR_SRPD                          (1 << 16)

/**** TYPES ****/

/**
 * @brief RX descriptor
 */
typedef struct e1000_rx_desc {
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint16_t checksum;
	volatile uint8_t  status;
	volatile uint8_t  errors;
	volatile uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

/**
 * @brief TX descriptor
 */
typedef struct e1000_tx_desc {
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint8_t  cso;
	volatile uint8_t  cmd;
	volatile uint8_t  status;
	volatile uint8_t  css;
	volatile uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

/**
 * @brief E1000 NIC
 */
typedef struct e1000 {
    pci_device_t *pci_device;                   // PCI device
    uint16_t nic_type;                          // Device ID field
    fs_node_t *nic;                             // NIC
	spinlock_t *lock;							// NIC lock

    uintptr_t mmio;                             // MMIO address

    int eeprom;                                 // Has EEPROM
    int link;                                   // Link status
    int tx_current;                             // Current Tx
    int rx_current;                             // Current Rx

    volatile e1000_tx_desc_t *tx_descs;         // Tx descriptors
    volatile e1000_rx_desc_t *rx_descs;         // Rx descriptors

    uintptr_t tx_virt[E1000_NUM_TX_DESC]; 		// Virtual addresses of Tx descriptor buffers
    uintptr_t rx_virt[E1000_NUM_RX_DESC]; 		// Virtual addresses of Rx descriptor buffers
} e1000_t;

#endif