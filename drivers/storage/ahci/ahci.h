/**
 * @file drivers/storage/ahci/ahci.h
 * @brief AHCI driver
 * 
 * @ref https://wiki.osdev.org/AHCI for structures
 * @ref https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef AHCI_H
#define AHCI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/clock.h>
#include <kernel/drivers/pci.h>

/**** DEFINITIONS ****/

// Status codes
#define AHCI_SUCCESS		0	// Success
#define AHCI_ERROR			1	// Error
#define AHCI_TIMEDOUT		2	// Timeout

// Flags
#define AHCI_READ			0
#define AHCI_WRITE			1	

// Device types
#define AHCI_DEVICE_NONE		0	// No device
#define AHCI_DEVICE_SATA		1	// SATA device
#define AHCI_DEVICE_SATAPI		2	// SATAPI device
#define AHCI_DEVICE_SEMB		3	// Enclosure management bridge
#define AHCI_DEVICE_PM			4	// Port multiplier

// SATA signatures
#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	    0x96690101	// Port multiplier

// FIS (Frame Information Structure) types
#define FIS_TYPE_REG_H2D    0x27    // Register FIS - host to device
#define FIS_TYPE_REG_D2H    0x34    // Register FIS - device to host
#define FIS_TYPE_DMA_ACT    0x39    // DMA activate FIS - bidirectional
#define FIS_TYPE_DMA_SETUP  0x41    // DMA setup FIS - bidirectional
#define FIS_TYPE_DATA       0x46    // Data FIS - bidirectional
#define FIS_TYPE_BIST       0x58    // BIST activate FIS - bidirectional
#define FIS_TYPE_PIO_SETUP  0x5F    // PIO setup FIS - device to host
#define FIS_TYPE_DEV_BITS   0XA1    // Set device bits FIS - device to host

// ATA Commands
#define ATA_CMD_READ_PIO          	0x20
#define ATA_CMD_READ_PIO_EXT      	0x24
#define ATA_CMD_READ_DMA          	0xC8
#define ATA_CMD_READ_DMA_EXT      	0x25
#define ATA_CMD_WRITE_PIO         	0x30
#define ATA_CMD_WRITE_PIO_EXT     	0x34
#define ATA_CMD_WRITE_DMA         	0xCA
#define ATA_CMD_WRITE_DMA_EXT     	0x35
#define ATA_CMD_CACHE_FLUSH       	0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   	0xEA
#define ATA_CMD_PACKET            	0xA0
#define ATA_CMD_IDENTIFY_PACKET   	0xA1
#define ATA_CMD_IDENTIFY          	0xEC

// (incomplete) List of ATAPI packet commands
#define ATAPI_TEST_UNIT_READY       0x00
#define ATAPI_REQUEST_SENSE         0x03
#define ATAPI_FORMAT_UNIT           0x04
#define ATAPI_START_STOP_UNIT       0x1B    // Eject
#define ATAPI_PREVENT_REMOVAL       0x1E    // Prevent removal
#define ATAPI_READ_CAPACITY         0x25    // Read capacity
#define ATAPI_SEEK                  0x2B    // Seek
#define ATAPI_WRITE_AND_VERIFY      0x2E    // Write and verify
#define ATAPI_READ                  0xA8    // Read (12)
#define ATAPI_WRITE                 0xAA    // Write (12)


// ATA statuses (PxTFD)
#define ATA_SR_BSY      0x80    // Busy
#define ATA_SR_DRDY     0x40    // Drive ready
#define ATA_SR_DF       0x20    // Drive write fault
#define ATA_SR_DSC      0x10    // Drive seek complete
#define ATA_SR_DRQ      0x08    // Data request  ready
#define ATA_SR_CORR     0x04    // Corrected data
#define ATA_SR_IDX      0x02    // Index
#define ATA_SR_ERR      0x01    // Error

// HBA capabilities
#define HBA_CAP_S64A				0x80000000	// Supports 64-bit Addressing
#define HBA_CAP_SNCQ				0x40000000	// Supports Native Command Queuing
#define HBA_CAP_SSNTF				0x20000000	// Supports SNotification Register
#define HBA_CAP_SMPS				0x10000000	// Supports Mechanical Presence Switch
#define HBA_CAP_SSS					0x08000000	// Supports Staggered Spin-up
#define HBA_CAP_SALP				0x04000000	// Supports Aggressive Link Power Management
#define HBA_CAP_SAL					0x02000000	// Supports Activity LED
#define HBA_CAP_SCLO				0x01000000	// Supports Command List Override
#define HBA_CAP_ISS					0x00F00000	// Interface Speed Support (4 bit value, see extra)
#define HBA_CAP_SAM					0x00040000	// Supports AHCI mode only
#define	HBA_CAP_SPM					0x00020000	// Supported Port Multiplier
#define HBA_CAP_FBSS				0x00010000	// FIS-based Switching Supported
#define HBA_CAP_PMD					0x00008000	// PIO Multiple DRQ Block
#define HBA_CAP_SSC					0x00004000	// Slumber State Capable
#define HBA_CAP_PSC					0x00002000	// Partial State Capable
#define HBA_CAP_NCS					0x00001F00	// Number of Command Slots (5 bit value, see extra)
#define HBA_CAP_CCCS				0x00000080	// Command Completion Coalescing Supported
#define HBA_CAP_EMS					0x00000040	// Enclosure Management Support
#define HBA_CAP_SXS					0x00000020	// Supports External SATA
#define HBA_CAP_NP					0x0000000F	// Number of Ports

// HBA capabilities (extra)
#define HBA_CAP_NCS_SHIFT			8
#define HBA_CAP_ISS_SHIFT			20

// HBA capabilities extended
#define HBA_CAP_EXT_BOH				0x00000001	// BIOS/OS Handoff

// HBA control
#define HBA_GHC_AE					0x80000000	// AHCI Enable
#define HBA_GHC_MRSM				0x00000004	// MSI Revert to Single Message
#define HBA_GHC_IE					0x00000002	// Interrupt Enable
#define HBA_GHC_HR					0x00000001	// HBA Reset

// HBA port - Command (PxCMD)
#define HBA_PORT_PXCMD_ICC			0xF0000000
#define HBA_PORT_PXCMD_ASP			0x08000000
#define HBA_PORT_PXCMD_ALPE			0x04000000
#define HBA_PORT_PXCMD_DLAE			0x02000000
#define HBA_PORT_PXCMD_ATAPI		0x01000000
#define HBA_PORT_PXCMD_APTSE		0x00800000
#define HBA_PORT_PXCMD_FBSCP		0x00400000
#define HBA_PORT_PXCMD_ESP			0x00200000
#define HBA_PORT_PXCMD_CPD			0x00100000
#define HBA_PORT_PXCMD_MPSP			0x00080000
#define HBA_PORT_PXCMD_HPCP			0x00040000
#define HBA_PORT_PXCMD_PMA			0x00020000
#define HBA_PORT_PXCMD_CPS			0x00010000
#define HBA_PORT_PXCMD_CR			0x00008000
#define HBA_PORT_PXCMD_FR			0x00004000
#define HBA_PORT_PXCMD_MPSS			0x00002000
#define HBA_PORT_PXCMD_CCS			0x00001F00
#define HBA_PORT_PXCMD_FRE			0x00000010
#define HBA_PORT_PXCMD_CLO			0x00000008
#define HBA_PORT_PXCMD_POD			0x00000004
#define HBA_PORT_PXCMD_SUD			0x00000002
#define HBA_PORT_PXCMD_ST			0x00000001

// HBA port - Serial ATA status (PxSSTS)
#define HBA_PORT_PXSSTS_IPM			0x00000F00	// Interface power management transitions allowed
#define HBA_PORT_PXSSTS_SPD			0x000000F0	// Speed allowed
#define HBA_PORT_PXSSTS_DET			0x0000000F	// Device detection initialization

// HBA port - Serial ATA control (PxSCTL)
#define HBA_PORT_PXSCTL_IPM			0x00000F00	// Interface power management transitions allowed
#define HBA_PORT_PXSCTL_SPD			0x000000F0	// Speed allowed
#define HBA_PORT_PXSCTL_DET			0x0000000F	// Device detection initialization

// Device Detection Initialization values (PxSSTS)
#define HBA_PORT_SSTS_DET_NONE			0x00	// No device detection and Phy communication not established
#define HBA_PORT_SSTS_DET_NO_PHY		0x01	// Device present but Phy communication not established
#define HBA_PORT_SSTS_DET_PRESENT		0x03	// Device present and Phy communication established
#define HBA_PORT_SSTS_DET_DISABLE		0x04	// Device disabled and offline

// Device Detection Initialization values (PxSCTL)
#define HBA_PORT_SCTL_DET_NONE			0x00	// No device detection or initialization action requested
#define HBA_PORT_SCTL_DET_RESET			0x01	// Perform hard reset
#define HBA_PORT_SCTL_DET_DISABLE		0x04	// Disable SATA interface and put Phy in offline mode

// IPM values (PxSSTS)
#define HBA_PORT_SSTS_IPM_NONE			0x00000000	// Device not present or communication not established
#define HBA_PORT_SSTS_IPM_ACTIVE		0x00000100	// Interface in active state
#define HBA_PORT_SSTS_IPM_PARTIAL		0x00000200	// Interface in partial state
#define HBA_PORT_SSTS_IPM_SLUMBER		0x00000600	// Interface in slumber state
#define HBA_PORT_SSTS_IPM_DEVSLEEP		0x00000800	// Interface in DevSleep state

// IPM values (PxSCTL)
#define HBA_PORT_SCTL_IPM_NO_RESTRICT	0x00000000	// No interface restrictions
#define HBA_PORT_SCTL_IPM_PARTIAL		0x00000100	// Transitions to partial state disabled
#define HBA_PORT_SCTL_IPM_SLUMBER		0x00000200	// Transitions to slumber state disabled
#define HBA_PORT_SCTL_IPM_DEVSLEEP		0x00000400	// Transitions to DevSleep state disabled

// HBA port - Interrupt Status (PxIS)
#define HBA_PORT_PXIS_CPDS				0x80000000	// Cold port detect status
#define HBA_PORT_PXIS_TFES				0x40000000	// Task file error status
#define HBA_PORT_PXIS_HBFS				0x20000000	// Host bus fatal error status
#define HBA_PORT_PXIS_HBDS				0x10000000	// Host bus data error status
#define HBA_PORT_PXIS_IFS				0x08000000	// Interface fatal error status
#define HBA_PORT_PXIS_INFS				0x04000000	// Interface non-fatal error status
#define HBA_PORT_PXIS_OFS				0x01000000	// Overflow status
#define HBA_PORT_PXIS_IPMS				0x00800000	// Incorrect port multiplier status
#define HBA_PORT_PXIS_PRCS				0x00400000	// PhyRdy change status
#define HBA_PORT_PXIS_DMPS				0x00000080	// Device mechanical presence status
#define HBA_PORT_PXIS_PCS				0x00000040	// Port connect change status
#define HBA_PORT_PXIS_DPS				0x00000020	// Descriptor processed
#define HBA_PORT_PXIS_UFS				0x00000010	// Unknown FIS interrupt
#define HBA_PORT_PXIS_SDBS				0x00000008	// Set device bits interrupt
#define HBA_PORT_PXIS_DSS				0x00000004	// DMA setup FIS interrupt
#define HBA_PORT_PXIS_PSS				0x00000002	// PIO setup FIS interrupt
#define HBA_PORT_PXIS_DHRS				0x00000001	// Device to host register FIS interrupt

// HBA port - SATA Error (PxSERR)
#define HBA_PORT_PXSERR_X				0x04000000	// Exchanged - Change in device presence detected
#define HBA_PORT_PXSERR_F				0x02000000	// Unknown FIS type
#define HBA_PORT_PXSERR_T				0x01000000	// Transport state transition error
#define HBA_PORT_PXSERR_S				0x00800000	// Link sequence error
#define HBA_PORT_PXSERR_H				0x00400000	// Handshake error
#define HBA_PORT_PXSERR_C				0x00200000	// CRC error
#define HBA_PORT_PXSERR_D				0x00100000	// Unused
#define HBA_PORT_PXSERR_B				0x00080000	// 10B to 8B decode error
#define HBA_PORT_PXSERR_W				0x00040000	// Comm wake
#define HBA_PORT_PXSERR_I				0x00020000	// Phy internal error
#define HBA_PORT_PXSERR_N				0x00010000	// PhyRdy change
#define HBA_PORT_PXSERR_ERR_E			0x00000800	// Internal error
#define HBA_PORT_PXSERR_ERR_P			0x00000400	// Protocol error
#define HBA_PORT_PXSERR_ERR_C			0x00000200	// Persistent communication or data integrity error
#define HBA_PORT_PXSERR_ERR_T			0x00000100	// Transient data integrity error
#define HBA_PORT_PXSERR_ERR_M			0x00000002	// Recovered communications error
#define HBA_PORT_PXSERR_ERR_I			0x00000001	// Recovered data integrity error

// Entry counts
#define AHCI_CMD_HEADER_COUNT		32		// Length of command list
#define AHCI_PRDT_COUNT 			168		// Length of PRDT

// PRD variables
#define AHCI_PRD_MAX_BYTES			0x400000	// 4MB

/**** TYPES ****/

/**
 * @brief FIS host-to-device structure
 */
typedef struct ahci_fis_h2d
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_REG_H2D

	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:3;	// Reserved
	uint8_t  c:1;		// 1: Command, 0: Control

	uint8_t  command;	// Command register
	uint8_t  featurel;	// Feature register, 7:0
	
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;    // Device register

	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  featureh;	// Feature register, 15:8

	// DWORD 3
	uint8_t  countl;	// Count register, 7:0
	uint8_t  counth;	// Count register, 15:8
	uint8_t  icc;		// Isochronous command completion
	uint8_t  control;	// Control register

	// DWORD 4
	uint8_t  rsv1[4];	// Reserved
} ahci_fis_h2d_t;

/**
 * @brief FIS device-to-host structure
 */
typedef struct ahci_fis_d2h
{
	// DWORD 0
	uint8_t  fis_type;    // FIS_TYPE_REG_D2H

	uint8_t  pmport:4;    // Port multiplier
	uint8_t  rsv0:2;      // Reserved
	uint8_t  i:1;         // Interrupt bit
	uint8_t  rsv1:1;      // Reserved

	uint8_t  status;      // Status register
	uint8_t  error;       // Error register
	
	// DWORD 1
	uint8_t  lba0;        // LBA low register, 7:0
	uint8_t  lba1;        // LBA mid register, 15:8
	uint8_t  lba2;        // LBA high register, 23:16
	uint8_t  device;      // Device register

	// DWORD 2
	uint8_t  lba3;        // LBA register, 31:24
	uint8_t  lba4;        // LBA register, 39:32
	uint8_t  lba5;        // LBA register, 47:40
	uint8_t  rsv2;        // Reserved

	// DWORD 3
	uint8_t  countl;      // Count register, 7:0
	uint8_t  counth;      // Count register, 15:8
	uint8_t  rsv3[2];     // Reserved

	// DWORD 4
	uint8_t  rsv4[4];     // Reserved
} ahci_fis_d2h_t;

/**
 * @brief Data FIS structure
 */
typedef struct ahci_fis_data
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_DATA

	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:4;	
    // Reserved

	uint8_t  rsv1[2];	// Reserved

	// DWORD 1 ~ N
	uint32_t data[];	// Payload
} ahci_fis_data_t;

/**
 * @brief PIO setup
 */
typedef struct ahci_fis_pio_setup
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP

	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;

	uint8_t  status;		// Status register
	uint8_t  error;		// Error register

	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register

	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved

	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3;		// Reserved
	uint8_t  e_status;	// New value of status register

	// DWORD 4
	uint16_t tc;		// Transfer count
	uint8_t  rsv4[2];	// Reserved
} ahci_fis_pio_setup_t;

/**
 * @brief DMA setup structure
 */
typedef struct ahci_fis_dma_setup
{
	// DWORD 0
	uint8_t  fis_type;	    // FIS_TYPE_DMA_SETUP

	uint8_t  pmport:4;	    // Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		    // Data transfer direction, 1 - device to host
	uint8_t  i:1;		    // Interrupt bit
	uint8_t  a:1;           // Auto-activate. Specifies if DMA Activate FIS is needed

    uint8_t  rsved[2];      // Reserved

	// DWORD 1&2

    uint64_t DMAbufferID;   // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
                            // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //DWORD 3
    uint32_t rsvd;          // Reserved

    // DWORD 4
    uint32_t DMAbufOffset;  // Byte offset into buffer. First 2 bits must be 0

    // DWORD 5
    uint32_t TransferCount; // Number of bytes to transfer. Bit 0 must be 0

    // DWORD 6
    uint32_t resvd;         // Reserved
} ahci_fis_dma_setup_t;

/**
 * @brief HBA port
 * @see Section 3.3 in the AHCI specification
 */
typedef volatile struct ahci_hba_port
{
	uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
	uint32_t clbu;		// 0x04, command list base address upper 32 bits
	uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
	uint32_t is;		// 0x10, interrupt status
	uint32_t ie;		// 0x14, interrupt enable
	uint32_t cmd;		// 0x18, command and status
	uint32_t rsv0;		// 0x1C, Reserved
	uint32_t tfd;		// 0x20, task file data
	uint32_t sig;		// 0x24, signature
	uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t serr;		// 0x30, SATA error (SCR1:SError)
	uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
	uint32_t ci;		// 0x38, command issue
	uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fbs;		// 0x40, FIS-based switch control
	uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} ahci_hba_port_t;

/**
 * @brief HBA memory tag
 */
typedef volatile struct ahci_hba_mem
{
	// 0x00 - 0x2B, Generic Host Control
	uint32_t cap;		// 0x00, Host capability
	uint32_t ghc;		// 0x04, Global host control
	uint32_t is;		// 0x08, Interrupt status
	uint32_t pi;		// 0x0C, Port implemented
	uint32_t vs;		// 0x10, Version
	uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
	uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
	uint32_t em_loc;	// 0x1C, Enclosure management location
	uint32_t em_ctl;	// 0x20, Enclosure management control
	uint32_t cap2;		// 0x24, Host capabilities extended
	uint32_t bohc;		// 0x28, BIOS/OS handoff control and status

	// 0x2C - 0x9F, Reserved
	uint8_t  rsv[0xA0-0x2C];

	// 0xA0 - 0xFF, Vendor specific registers
	uint8_t  vendor[0x100-0xA0];

	// 0x100 - 0x10FF, Port control registers
	ahci_hba_port_t	ports[32];	// 1 ~ 32
} ahci_hba_mem_t;

/**
 * @brief Received FIS
 * @see Section 4.2.1 in the AHCI specification
 */
typedef volatile struct ahci_received_fis
{
	// 0x00
	ahci_fis_dma_setup_t	dsfis;		// DMA Setup FIS
	uint8_t         		pad0[4];

	// 0x20
	ahci_fis_pio_setup_t	psfis;		// PIO Setup FIS
	uint8_t         pad1[12];

	// 0x40
	ahci_fis_d2h_t			rfis;		// Register – Device to Host FIS
	uint8_t         		pad2[4];

	// 0x58
	uint8_t					sdbfis[8];	// Set Device Bits FIS (UNIMPL)
	
	// 0x60
	uint8_t         ufis[64];

	// 0xA0
	uint8_t   	rsv[0x100-0xA0];
} ahci_received_fis_t;

/**
 * @brief HBA command header
 */
typedef struct ahci_cmd_header
{
	// DW0
	uint8_t  cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	uint8_t  a:1;		// ATAPI
	uint8_t  w:1;		// Write, 1: H2D, 0: D2H
	uint8_t  p:1;		// Prefetchable

	uint8_t  r:1;		// Reset
	uint8_t  b:1;		// BIST
	uint8_t  c:1;		// Clear busy upon R_OK
	uint8_t  rsv0:1;		// Reserved
	uint8_t  pmp:4;		// Port multiplier port

	uint16_t prdtl;		// Physical region descriptor table length in entries

	// DW1
	volatile
	uint32_t prdbc;		// Physical region descriptor byte count transferred

	// DW2, 3
	uint32_t ctba;		// Command table descriptor base address
	uint32_t ctbau;		// Command table descriptor base address upper 32 bits

	// DW4 - 7
	uint32_t rsv1[4];	// Reserved
} ahci_cmd_header_t;

/**
 * @brief Entry in the PRDT (Physical Region Descriptor Table)
 */
typedef struct ahci_prdt_entry
{
	uint32_t dba;		// Data base address
	uint32_t dbau;		// Data base address upper 32 bits
	uint32_t rsv0;		// Reserved

	// DW3
	uint32_t dbc:22;	// Byte count, 4M max
	uint32_t rsv1:9;	// Reserved
	uint32_t i:1;		// Interrupt on completion
} ahci_prdt_entry_t;


/**
 * @brief Command table
 */
typedef struct ahci_cmd_table
{
	// 0x00
	uint8_t  cfis[64];	// Command FIS

	// 0x40
	uint8_t  acmd[16];	// ATAPI command, 12 or 16 bytes

	// 0x50
	uint8_t  rsv[48];	// Reserved

	// 0x80
	ahci_prdt_entry_t	prdt_entry[AHCI_PRDT_COUNT];      // Physical region descriptor table entries, 0 ~ 65535
} ahci_cmd_table_t;



/**
 * @brief ATA identification space
 * 
 * @see https://hddguru.com/documentation/2006.01.27-ATA-ATAPI-8-rev2b/ table 12 for identificatoin space
 */
typedef struct ata_ident {
    uint16_t flags;             // If bit 15 is cleared, valid drive. If bit 7 is set to one, this is removable.
    uint16_t obsolete;          // Obsolete
    uint16_t specifics;         // 7.17.7.3 in specification
    uint16_t obsolete2[6];      // Obsolete
    uint16_t obsolete3;         // Obsolete
    char serial[20];            // Serial number
    uint16_t obsolete4[3];      // Obsolete
    char firmware[8];           // Firmware revision
    char model[40];             // Model number
    uint16_t rw_multiple;       // R/W multiple support (<=16 is SATA)
    uint16_t obsolete5;         // Obsolete
    uint32_t capabilities;      // Capabilities of the IDE device
    uint16_t obsolete6[2];      // Obsolete
    uint16_t field_validity;    // If 1, the values reported in _ - _ are valid
    uint16_t obsolete7[5];      // Obsolete
    uint16_t multi_sector;      // Multiple sector setting
    uint32_t sectors;           // Total addressible sectors
    uint16_t obsolete8[20];     // Technically these aren't obsolete, but they contain nothing really useful
    uint32_t command_sets;      // Command/feature sets
    uint16_t obsolete9[16];     // Contain nothing really useful
    uint64_t sectors_lba48;     // LBA48 maximum sectors, AND by 0000FFFFFFFFFFFF for validity
    uint16_t obsolete10[2];     // Contain nothing really useful
    uint16_t phys_to_logical;   // Confirms physical sector size to logical sector size
    uint16_t obsolete11[11];   	// Contain nothing really useful
	uint32_t sector_size;		// Logical sector size
	uint16_t obsolete12[136];	// Nothing really useful
} __attribute__((packed)) __attribute__((aligned(8))) ata_ident_t;


// Prototype
struct ahci;

/**
 * @brief AHCI port structure (internal to driver)
 */
typedef struct ahci_port {
	// GENERAL
	struct ahci *parent;			// Parent controller
	int port_num;					// Number of the port
	int type;						// Type of the device connected
	uint64_t size;              	// Size of the device in bytes

	// DMA
	uintptr_t dma_buffer;			// DMA buffer for quick access

	// ATA
	ata_ident_t *ident;				// Identification space

    // ATAPI
    uint64_t atapi_block_size;  	// Block size for ATAPI

	// PORT SPECIFICS
	ahci_hba_port_t *port;			// HBA port structure (registers)
	ahci_received_fis_t *fis;		// FIS receive area
	ahci_cmd_header_t *cmd_list;	// Command list
	ahci_cmd_table_t *cmd_table;	// Command table
} ahci_port_t;

/**
 * @brief AHCI controller structure (internal to driver)
 */
typedef struct ahci {
	ahci_hba_mem_t *mem;		// HBA memory
	pci_device_t *pci_device;	// PCI device of controller

	int ncmdslot;				// Number of command slots
	ahci_port_t *ports[32];		// Allocated list of port structures
} ahci_t;



/**** MACROS ****/

/* ISO C forbids blah blah blah */
#pragma GCC diagnostic ignored "-Wpedantic"

/* Timeout macro - really should be in base kernel */
#define AHCI_TIMEOUT(condition, timeout) \
									({							\
										int time = timeout; 		\
										while (time) { 				\
											time--;					\
											if (condition) break;	\
										} 							\
										(time <= 0)?1:0;			\
									})

/* Get bits */
#define AHCI_LOW(var) (uint32_t)(var & 0xFFFFFFFF)
#define AHCI_HIGH(var) (uint32_t)(((uint64_t)var >> 32) & 0xFFFFFFFF)

/* Check alignment */
#define AHCI_ALIGNED(var, alignment) !((uintptr_t)var & (alignment-1))

/* Virtual to physical, but quicker */
#define AHCI_SET_ADDRESS(var, address) 	{	\
											uintptr_t phys = arch_mmu_physical(NULL, (uintptr_t)address);		\
											var = AHCI_LOW(phys);		\
											(var##u) = AHCI_HIGH(phys);	\
										}

/* Reorder bytes macro (for ATA identification space) */
#define ATA_REORDER_BYTES(buffer, size)     for (int i = 0; i < size-1; i+=2) { uint8_t tmp = ((uint8_t*)buffer)[i+1]; ((uint8_t*)buffer)[i+1] = ((uint8_t*)buffer)[i]; ((uint8_t*)buffer)[i] = tmp; }



/**** FUNCTIONS ****/

/**
 * @brief Initialize a port
 * @param ahci AHCI controller
 * @param port_number The number of the port to initialize
 * @returns @c ahci_port_t structure or NULL
 */
ahci_port_t *ahci_portInitialize(ahci_t *ahci, int port_number);

/**
 * @brief Finish port initialization
 * @param port The port to finish initializing
 * @returns 0 on success
 */
int ahci_portFinishInitialization(ahci_port_t *port);

/**
 * @brief Deinitialize a port entirely
 * @param port The port to deinitialize
 */
void ahci_portDeinitialize(ahci_port_t *port);

/**
 * @brief Execute a request for a specific port
 * @param port The port to execute the request for
 * @param operation The operation to do
 * @param lba The LBA
 * @param sectors The amount of sectors to operate on
 * @param buffer The buffer to use (assume DMA)
 * @returns Error code
 */
int ahci_portOperate(ahci_port_t *port, int operation, uint64_t lba, size_t sectors, uint8_t *buffer);

/**
 * @brief Handle a port IRQ
 * @param port The port
 */
void ahci_portIRQ(ahci_port_t *port);

/**
 * @brief VFS read method for AHCI device
 * @warning !!! THIS MATH IS HORRIBLE DO NOT READ !!!
 */
ssize_t ahci_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);

/**
 * @brief VFS write method for AHCI device
 * @warning !!! HORRIBLE CODING !!!
 */
ssize_t ahci_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);

/**
 * @brief Allocate a new filesystem node for AHCI
 */ 
fs_node_t *ahci_createNode(ahci_port_t *port);

#endif