/**
 * @file drivers/ide/ata.h
 * @brief ATA components of the IDE driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_IDE_ATA_H
#define DRIVERS_IDE_ATA_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>

/**** TYPES ****/

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
    uint16_t obsolete10[152];   // Contain nothing really useful
} __attribute__((packed)) __attribute__((aligned(8))) ata_ident_t;

/**
 * @brief IDE channel
 */
typedef struct ide_channel {
    uint32_t io_base;           // I/O base of the drive
    uint32_t control;           // Control base of the drive
    uint32_t bmide;             // Bus mastering IDE base
    uint8_t nIEN;               // nIEN (No Interrupt)
} ide_channel_t;

/**
 * @brief IDE device (can be ATA or ATAPI)
 */
typedef struct ide_device {
    int exists;                 // Does the drive even exist?
    int channel;                // Channel the drive is on (ATA_PRIMARY or ATA_SECONDARY) - if -1 the device is ignored
    int slave;                  // Is the drive a slave?
    int atapi;                  // Is the drive ATAPI?

    ata_ident_t ident;          // Identification space
    uint64_t size;              // Size of the device in bytes

    // ATAPI
    uint64_t atapi_block_size;  // Block size for ATAPI

    // Identification space stuff (but null-terminated and good)
    char model[41];             // Model number of the drive
    char serial[21];            // Serial number of the drive
    char firmware[9];           // Firmware of the drive
} ide_device_t;

/**
 * @brief ATAPI packet union
 */
typedef union atapi_packet {
    uint8_t bytes[12];
    uint16_t words[6];
} atapi_packet_t;

/**** DEFINITIONS ****/

// Status register bitflags
#define ATA_SR_BSY      0x80    // Busy
#define ATA_SR_DRDY     0x40    // Drive ready
#define ATA_SR_DF       0x20    // Drive write fault
#define ATA_SR_DSC      0x10    // Drive seek complete
#define ATA_SR_DRQ      0x08    // Data request  ready
#define ATA_SR_CORR     0x04    // Corrected data
#define ATA_SR_IDX      0x02    // Index
#define ATA_SR_ERR      0x01    // Error

// Features/error port
#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

// Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// Identification space
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// Interface type
#define IDE_ATA                 0x00

// ATA channels
#define ATA_PRIMARY             0
#define ATA_SECONDARY           1

// ATA devices
#define ATA_MASTER              0
#define ATA_SLAVE               1

// Directions
#define ATA_READ                0x00
#define ATA_WRITE               0x01

// ATA registers (offsets from BAR0 and/or BAR2)
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A    
#define ATA_REG_LBA5       0x0B     
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

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


// ATA PCI device
#define ATA_PCI_TYPE        0x0101  // Mass Storage Controller of type IDE Controller

// Base I/O addresses
#define ATA_PRIMARY_BASE        0x1F0
#define ATA_PRIMARY_CONTROL     0x3F6
#define ATA_SECONDARY_BASE      0x170
#define ATA_SECONDARY_CONTROL   0x376

// Return values of IDE functions
#define IDE_SUCCESS             0   // No error
#define IDE_DEVICE_FAULT        1   // Device fault
#define IDE_ERROR               2   // ATA error
#define IDE_DRQ_NOT_SET         3   // Drive request not set
#define IDE_TIMEOUT             4   // Timeout

/**** MACROS ****/

#define LOG(status, ...) dprintf_module(status, "DRIVER:IDE", __VA_ARGS__)

/**** FUNCTIONS ****/

/**
 * @brief Initialize the ATA/ATAPI driver logic
 */
int ata_initialize();

#endif