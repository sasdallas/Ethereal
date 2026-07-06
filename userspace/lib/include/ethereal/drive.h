/**
 * @file userspace/lib/include/ethereal/drive.h
 * @brief Ethereal drive ioctl callbacks
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _ETHEREAL_DRIVE_H
#define _ETHEREAL_DRIVE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define DRIVE_TYPE_UNKNOWN      0   // Unknown
#define DRIVE_TYPE_IDE_HD       1   // IDE harddrive
#define DRIVE_TYPE_CDROM        2   // CD-ROM (use DRIVE_TYPE_SCSI_CDROM for SCSI CDROMs)
#define DRIVE_TYPE_SATA         3   // SATA drive
#define DRIVE_TYPE_SCSI         4   // SCSI drive
#define DRIVE_TYPE_SCSI_CDROM   5   // SCSI CD-rom
#define DRIVE_TYPE_NVME         6   // NVMe drive
#define DRIVE_TYPE_FLOPPY       7   // Floppy drive
#define DRIVE_TYPE_MMC          8   // MMC drive

#define DRIVE_NAME_IDE_HD       "idehd"
#define DRIVE_NAME_CDROM        "cdrom"
#define DRIVE_NAME_SATA         "sata"
#define DRIVE_NAME_SCSI         "scsi"
#define DRIVE_NAME_SCSI_CDROM   "scsicd"
#define DRIVE_NAME_NVME         "nvme"
#define DRIVE_NAME_FLOPPY       "floppy"
#define DRIVE_NAME_MMC          "mmc"
#define DRIVE_NAME_UNKNOWN      "unknown"

#define IO_DRIVE_GET_INFO       0x5001
#define IO_DRIVE_GET_MODEL      0x5002
#define IO_DRIVE_GET_SERIAL     0x5003
#define IO_DRIVE_GET_REV        0x5004
#define IO_DRIVE_GET_VENDOR     0x5005
#define IO_DRIVE_RESCAN_PART    0x5006

/**** TYPES ****/

typedef struct drive_info {
    size_t sectors;
    size_t sector_size;
    unsigned char type;
    
    // todo bios geom, logical sector size, etc.
} drive_info_t;

#endif
