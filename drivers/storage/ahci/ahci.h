/**
 * @file drivers/storage/ahci/ahci.h
 * @brief AHCI
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _AHCI_H
#define _AHCI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/tasklet.h>
#include <kernel/misc/util.h>
#include <kernel/misc/waitqueue.h>
#include <kernel/drivers/pci.h>
#include <structs/bitmap.h>

/**** DEFINITIONS ****/

#define AHCI_FIS_TYPE_REG_H2D           0x27
#define AHCI_FIS_TYPE_REG_D2H           0x34
#define AHCI_FIS_TYPE_DMA_ACT           0x39
#define AHCI_FIS_TYPE_DMA_SETUP         0x41
#define AHCI_FIS_TYPE_DATA              0x46
#define AHCI_FIS_TYPE_BIST              0x58
#define AHCI_FIS_TYPE_PIO_SETUP         0x5F
#define AHCI_FIS_TYPE_DEV_BITS          0xA1

#define ATA_CMD_READ_DMA                0xC8
#define ATA_CMD_READ_DMA_EXT            0x25
#define ATA_CMD_WRITE_DMA               0xCA
#define ATA_CMD_WRITE_DMA_EXT           0x35
#define ATA_CMD_IDENTIFY                0xEC

#define	SATA_SIG_ATA	                0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	                0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	                0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	                    0x96690101	// Port multiplier

/* bits */
#define AHCI_GHC_HR                     (1 << 0)
#define AHCI_GHC_IE                     (1 << 1)
#define AHCI_GHC_AE                     (1 << 31)

#define AHCI_CAP_64BIT                  (1 << 31)
#define AHCI_CAP_SSS                    (1 << 27)
#define AHCI_CAP_NCS(cap)               ((((cap) >> 8) & 0x1f) + 1)
#define AHCI_CAP_NPRT(cap)              ((cap) & 0xF)

#define AHCI_CAP2_BOH                   (1 << 0)

#define AHCI_BOH_BOS                    (1 << 0)
#define AHCI_BOH_OOS                    (1 << 1)
#define AHCI_BOH_BB                     (1 << 4)

#define AHCI_PORT_CMD_ST                (1 << 0)
#define AHCI_PORT_CMD_SUD               (1 << 1)
#define AHCI_PORT_CMD_POD               (1 << 2)
#define AHCI_PORT_CMD_FRE               (1 << 4)
#define AHCI_PORT_CMD_CCS(pxcmd)        (((pxcmd) & 0x1f00) >> 8)
#define AHCI_PORT_CMD_FR                (1 << 14)
#define AHCI_PORT_CMD_CR                (1 << 15)
#define AHCI_PORT_CMD_ATAPI             (1 << 24)

#define AHCI_PORT_SCTL_DET(sctl)        ((sctl) & 0xF)

#define AHCI_PORT_SSTS_DET(pxssts)      (((pxssts) & 0xF))
#define AHCI_PORT_SSTS_SPD(pxssts)      (((pxssts) & 0xF0) >> 4)
#define AHCI_PORT_SSTS_IPM(pxssts)      (((pxssts) & 0xF00) >> 8)

#define AHCI_PORT_TFD_ERR(tfd)          (((tfd) & 0xF0) >> 8)

#define AHCI_PORT_TFD_STS_ERR           (1 << 0)
#define AHCI_PORT_TFD_STS_DRQ           (1 << 3)
#define AHCI_PORT_TFD_STS_BSY           (1 << 7)

#define AHCI_PORT_IE_DEFAULT            0xFC000000

#define AHCI_PORT_IS_TFES               (1 << 30)
#define AHCI_PORT_IS_HBFS               (1 << 29)
#define AHCI_PORT_IS_HBDS               (1 << 28)
#define AHCI_PORT_IS_IFS                (1 << 27)
#define AHCI_PORT_IS_INFS               (1 << 26)
#define AHCI_PORT_IS_OFS                (1 << 24)
#define AHCI_PORT_IS_IPMS               (1 << 23)
#define AHCI_PORT_IS_UFS                (1 << 4)

#define AHCI_PORT_DET_PRESENT           3
#define AHCI_PORT_IPM_ACTIVE            1

/**** TYPES ****/


typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsvd0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsvd1[11];
    uint32_t vendor[4];
} __attribute__((packed)) ahci_hba_port_t;

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_ports;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
} __attribute__((packed)) ahci_ghc_t;


#define GHC_PORT(ghc,i) ((volatile ahci_hba_port_t*)((uintptr_t)(ghc) + 0x100 + ((i) * 0x80)))

typedef struct {
    uint32_t base_lo;
    uint32_t base_hi;
    uint32_t rsvd0;
    uint32_t dbc:22;
    uint32_t rsvd1:9;
    uint32_t ioc:1;
} __attribute__((packed)) ahci_prdt_t;

typedef struct {
    uint8_t type;
    uint8_t pmport:4;
    uint8_t rsvd0:3;
    uint8_t c:1;

    uint8_t command;
    uint8_t feature1;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;

    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;

    uint8_t rsvd1[4];
} __attribute__((packed)) ahci_fis_h2d_t;

typedef struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsvd0:2;
    uint8_t i:1;
    uint8_t rsvd1:1;

    uint8_t sts;
    uint8_t err;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsvd2;

    uint8_t countl;
    uint8_t counth;
    uint8_t rsvd3[2];

    uint8_t rsvd4[4];
} __attribute__((packed)) ahci_fis_d2h_t;

typedef struct {
    uint8_t fis_type;
    uint8_t pmport:4;
    uint8_t rsvd0:4;

    uint8_t rsvd1[2];

    uint32_t data[];
} __attribute__((packed)) ahci_fis_data_t;

typedef struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsvd0:1;
    uint8_t d:1;
    uint8_t i:1;
    uint8_t rsvd1:1;

    uint8_t sts;
    uint8_t err;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsvd2;

    uint8_t countl;
    uint8_t counth;
    uint8_t rsvd3;
    uint8_t e_status;

    uint16_t tc;
    uint8_t rsvd4[2];
} __attribute__((packed)) ahci_fis_pio_setup_t;

typedef struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsvd0:1;
    uint8_t d:1;
    uint8_t i:1;
    uint8_t a:1;

    uint8_t rsvd1[2];

    uint64_t buf;
    uint32_t rsvd2;
    uint32_t off;
    uint32_t tc;
    uint32_t rsvd3;
} __attribute__((packed)) ahci_fis_dma_setup_t;

typedef struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsvd0:2;
    uint8_t i:1;
    uint8_t n:1;

    uint8_t status;
    uint8_t err;

    uint32_t rsvd1;
} __attribute__((packed)) ahci_fis_set_device_bits_t;

typedef struct ahci_received_fis {
    ahci_fis_dma_setup_t dsfis;
    uint8_t pad0[4];

    ahci_fis_pio_setup_t psfis;
    uint8_t pad1[12];

    ahci_fis_d2h_t rfis;
    uint8_t pad2[4];

    ahci_fis_set_device_bits_t sdbfis;
    
    uint8_t ufis[64];

    uint8_t rsvd0[0x100-0xa0];
} __attribute__((packed)) ahci_received_fis_t;

STATIC_ASSERT(sizeof(ahci_received_fis_t) == 0x100);

typedef struct ahci_cmd_header {
    uint8_t cfl:5;
    uint8_t a:1;
    uint8_t w:1;
    uint8_t p:1;

    uint8_t r:1;
    uint8_t b:1;
    uint8_t c:1;
    uint8_t rsvd0:1;
    uint8_t pmp:4;

    uint16_t prdtl;
    volatile uint32_t prdbc;

    uint32_t ctba;
    uint32_t ctbau;
    
    uint32_t rsvd1[4];
} __attribute__((packed)) ahci_cmd_header_t;

typedef struct ahci_cmd_tbl {
    ahci_fis_h2d_t cfis;
    uint8_t pad[44];
    uint8_t acmd[16];
    uint8_t rsvd0[48];
    ahci_prdt_t prdts[];
} __attribute__((packed)) ahci_cmd_tbl_t;

STATIC_ASSERT(sizeof(ahci_cmd_tbl_t) == 128);

/**
 * @brief ATA identification space
 * 
 * @see https://hddguru.com/documentation/2006.01.27-ATA-ATAPI-8-rev2b/ table 12 for identificatoin space
 */
typedef struct ahci_ident {
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
} __attribute__((packed)) ahci_ident_t;


struct ahci;

typedef struct ahci_port {
    int num;
    struct ahci *controller;
    volatile ahci_hba_port_t *regs;
    ahci_ident_t ident;

    uintptr_t cmd_region;
    tasklet_t tasklet;
    wait_queue_t slot_wait;
    struct thread *waiters[32];
    
    uint32_t slot_bitmap;
    uint32_t slot_errors;
    uint32_t slot_complete; // hack, only used a few times.
} ahci_port_t;

typedef struct ahci {
    pci_device_t *pcidev;
    volatile ahci_ghc_t *ghc;
    uintptr_t fis_region;
    int nslots;
    int nports;
    uint32_t enabled_ports;
    ahci_port_t *ports[32];
} ahci_t;

/**** MACROS ****/

#define AHCI_PRDT_SET_BASE(cmdtbl,prdt,base) (cmdtbl)->prdts[(prdt)].base_lo = ((uintptr_t)((uintptr_t)(base))) & 0xFFFFFFFF;\
                                             (cmdtbl)->prdts[(prdt)].base_hi = ((uintptr_t)((base)) >> 32) & 0xFFFFFFFF;

#define AHCI_PRDT_SET_BYTES(cmdtbl,prdt,bytes) (cmdtbl)->prdts[(prdt)].dbc = (bytes)-1
#define AHCI_PRDT_SET_IOC(cmdtbl,prdt,c) (cmdtbl)->prdts[(prdt)].ioc = (c)

#define AHCI_CMDHDR_LINK_TBL(cmdhdr,tbl)    (cmdhdr)->ctba = ((uintptr_t)(arch_mmu_physical(NULL, (uintptr_t)(tbl)))) & 0xFFFFFFFF;\
                                            (cmdhdr)->ctbau = ((uintptr_t)(arch_mmu_physical(NULL, (uintptr_t)(tbl))) >> 32) & 0xFFFFFFFF;



#endif
