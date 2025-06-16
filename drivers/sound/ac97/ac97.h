/**
 * @file drivers/sound/ac97/ac97.h
 * @brief AC/97
 * 
 * @ref https://web.archive.org/web/20170226174953im_/http://www.intel.com/content/dam/doc/manual/io-controller-hub-7-hd-audio-ac97-manual.pdf
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _AC97_H
#define _AC97_H

/**** INCLUDES ****/
#include <kernel/drivers/sound/card.h>
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

/* Registers */
#define AC97_REG_RESET                  0x00    // Reset
#define AC97_REG_MASTER_VOLUME          0x02    // Master volume
#define AC97_REG_AUX_VOLUME             0x04    // Auxiliary volume
#define AC97_REG_MICROPHONE_VOLUME      0x0E    // Microphone volume
#define AC97_REG_PCM_OUTPUT_VOLUME      0x18    // PCM output volume
#define AC97_REG_RECORD_SELECT          0x1A    // Record select
#define AC97_REG_EXTENDED               0x28    // Extended audio
#define AC97_REG_EXTENDED_CTRL          0x2A    // Extended audio control/stat
#define AC97_REG_PCM_FRONTDAC_RATE      0x2C    // Front DAC sample rate
#define AC97_REG_PCM_SURDAC_RATE        0x2E    // Surround DAC sample rate
#define AC97_REG_PCM_LFEDAC_RATE        0x30    // LFE DAC sample rate 
#define AC97_REG_PCM_LRADC_RATE         0x32    // L/R ADC sample rate

/* Bus mastering registers */
#define AC97_PI_BDBAR                   0x00    // PCM In Buffer Descriptor list base address
#define AC97_PI_CIV                     0x04    // PCM In Current Index Value
#define AC97_PI_LVI                     0x05    // PCM In Last Valid Index
#define AC97_PI_SR                      0x06    // PCM In Status
#define AC97_PI_PICB                    0x08    // PCM In Position in Current Buffer
#define AC97_PI_PIV                     0x0A    // PCM In Prefetched Index Value
#define AC97_PI_CR                      0x0B    // PCM In Control
#define AC97_PO_BDBAR                   0x10    // PCM Out Buffer Descriptor list base address
#define AC97_PO_CIV                     0x14    // PCM Out Current Index Value
#define AC97_PO_LVI                     0x15    // PCM Out Last Valid Index
#define AC97_PO_SR                      0x16    // PCM Out Status
#define AC97_PO_PICB                    0x18    // PCM Out Position in Current Buffer
#define AC97_PO_PIV                     0x1A    // PCM Out Prefetched Index Value
#define AC97_PO_CR                      0x1B    // PCM Out Control

/* AC97_Px_SR bits */
#define AC97_SR_DMA_HALTED              (1 << 0)    // DMA transfer halted (not due to an error)
#define AC97_SR_END                     (1 << 1)    // End of transfer
#define AC97_SR_LBE_INT                 (1 << 2)    // Last buffer entry transmitted interrupt
#define AC97_SR_IOC_INT                 (1 << 3)    // Interrupt on Complete transmitted interrupt
#define AC97_SR_FIFO_ERR_INT            (1 << 4)    // FIFO error interrupt

/* AC97_Px_CR bits */
#define AC97_CR_DMA                     (1 << 0)    // DMA control
#define AC97_CR_RESET                   (1 << 1)    // Reset this register box
#define AC97_CR_LBE                     (1 << 2)    // Last buffer entry interrupt enable
#define AC97_CR_IOC                     (1 << 3)    // Interrupt on complete interrupt enable
#define AC97_CR_FIFO_ERR                (1 << 4)    // FIFO error interrupt enable

/* Buffer descriptor list */
#define AC97_BDL_ENTRY_COUNT            32          // 32 entries in list
#define AC97_BDL_MAX_SAMPLE_COUNT       0xFFFE      // Maximum sample count
#define AC97_BDL_CTRL_BUP               (1 << 14)   // End
#define AC97_BDL_CTRL_IOC               (1 << 15)   // Interrupt on transfer

/* Driver-specific */
#define AC97_BDL_SIZE                   PAGE_SIZE*2 // 4 KB max of samples (each sample is 16-bit)
#define AC97_BDL_SAMPLES                AC97_BDL_SIZE/sizeof(uint16_t)


/**** TYPES ****/

typedef struct ac97_buffer_entry {
    uint32_t buffer;                // Physical address to send data
    uint16_t samples;               // Samples in the buffer
    uint16_t control;               // Control
} __attribute__((packed)) ac97_buffer_entry_t;

typedef struct ac97 {
    // PCI
    uint32_t pci_device;                            // PCI device
    
    // I/O
    uintptr_t io_base;                              // I/O base
    uintptr_t bm_io_base;                           // Bus mastering I/O base
    uint8_t io_type;                                // Type of I/O (0 = MMIO, 1 = actual I/O space)
    
    // BDL
    size_t idx;                                     // Last valid index
    ac97_buffer_entry_t *bdl;                       // BDL (virtual)
    uint32_t *bdl_buffers[AC97_BDL_ENTRY_COUNT];    // BDL allocated buffers
    
    // OTHER
    sound_card_t *card;                             // Card
} ac97_t;

#endif