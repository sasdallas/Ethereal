/**
 * @file drivers/sound/ac97/ac97.c
 * @brief AC/97 sound card driver
 * 
 * @todo Knobs
 * @todo Support microphone
 * 
 * This driver works with the sound stack by putting the AC/97 in a perpetual state of running around with no data,
 * and requesting the sound system for new data. If the card gets data it will be copied in and freed.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "ac97.h"
#include <kernel/drivers/sound/mixer.h>
#include <kernel/arch/arch.h>
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/debug.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:AC97", __VA_ARGS__)


/* AC/97 write methods */
#define AC97_WRITE8(reg, value) ac97_writeRegister(ac, reg, (uint8_t)value, 1, 0);
#define AC97_WRITE16(reg, value) ac97_writeRegister(ac, reg, (uint16_t)value, 2, 0);
#define AC97_WRITE32(reg, value) ac97_writeRegister(ac, reg, (uint32_t)value, 4, 0);
#define AC97_WRITE_BM8(reg, value) ac97_writeRegister(ac, reg, (uint8_t)value, 1, 1);
#define AC97_WRITE_BM16(reg, value) ac97_writeRegister(ac, reg, (uint16_t)value, 2, 1);
#define AC97_WRITE_BM32(reg, value) ac97_writeRegister(ac, reg, (uint32_t)value, 4, 1);

/* AC/97 read methods */
#define AC97_READ8(reg) ((uint8_t)ac97_readRegister(ac, reg, 1, 0))
#define AC97_READ16(reg) ((uint16_t)ac97_readRegister(ac, reg, 2, 0))
#define AC97_READ32(reg) ((uint32_t)ac97_readRegister(ac, reg, 4, 0))
#define AC97_READ_BM8(reg) ((uint8_t)ac97_readRegister(ac, reg, 1, 1))
#define AC97_READ_BM16(reg) ((uint16_t)ac97_readRegister(ac, reg, 2, 1))
#define AC97_READ_BM32(reg) ((uint32_t)ac97_readRegister(ac, reg, 4, 1))


/**
 * @brief AC/97 write to register
 * @param ac AC/97 card
 * @param reg The register to write to
 * @param value The value to write
 * @param size Size to write
 * @param bm Write to bus master
 */
void ac97_writeRegister(ac97_t *ac, uint16_t reg, uint32_t value, int size, int bm) {
    uintptr_t addr = (bm ? (ac->bm_io_base + reg) : (ac->io_base + reg));

    switch (size) {
        case 1:
            if (ac->io_type) outportb((unsigned short)addr, (uint8_t)value);
            else *((uint8_t*)addr) = (uint8_t)value;
            return;
        case 2:
            if (ac->io_type) outportw((unsigned short)addr, (uint16_t)value);
            else *((uint16_t*)addr) = (uint16_t)value;
            return;
        case 4:
        default:
            if (ac->io_type) outportl((unsigned short)addr, (uint32_t)value);
            else *((uint32_t*)addr) = (uint32_t)value;
            return;
    }
}

/**
 * @brief AC/97 read from register
 * @param ac AC/97 card
 * @param reg The register to read
 * @param size Size to read
 * @param bm Read from bus master
 */
uint32_t ac97_readRegister(ac97_t *ac, uint16_t reg, int size, int bm) {
    uintptr_t addr = (bm ? (ac->bm_io_base + reg) : (ac->io_base + reg));

    switch (size) {
        case 1:
            if (ac->io_type) return (uint32_t)inportb((unsigned short)addr);
            else return (uint32_t)*((uint8_t*)addr);
        case 2:
            if (ac->io_type) return (uint32_t)inportw((unsigned short)addr);
            else return (uint32_t)*((uint16_t*)addr);
        case 4:
        default:
            if (ac->io_type) return (uint32_t)inportl((unsigned short)addr);
            else return *((uint32_t*)addr);
    }
}

/**
 * @brief AC/97 IRQ handler
 */
int ac97_irq(void *data) {
    ac97_t *ac = (ac97_t*)data;

    // Why did we get an IRQ? Check PCM Out
    uint16_t status = AC97_READ_BM16(AC97_PO_SR);
    if (status & AC97_SR_IOC_INT) {
        // A transfer completed (ack the bit first)
        AC97_WRITE_BM16(AC97_PO_SR, AC97_SR_IOC_INT);

        // Can we get any data from the sound system?
        sound_card_buffer_data_t *data = mixer_buffer(ac->card);
        uint16_t target_buffer = ((AC97_READ_BM8(AC97_PO_CIV) + 2) & (AC97_BDL_ENTRY_COUNT-1));

        if (data) {
            // Align sample count (else QEMU will get stuck)
            size_t samples = data->size/2;
            size_t samples_aligned = samples & 0xFF ? (samples + 0xFF) & ~0xFF : samples;

            // Put it ahead
            ac->bdl[target_buffer].samples = samples_aligned;
            memcpy(ac->bdl_buffers[target_buffer], data->data, data->size);

            // Update the LVI so the card can keep processing
            AC97_WRITE_BM8(AC97_PO_LVI, target_buffer);

            kfree(data);
        }
        else {
            // Just update the LVI so the card will keep going in a loop
            ac->bdl[target_buffer].samples = 0x1000;
            memset(ac->bdl_buffers[target_buffer], 0x00, AC97_BDL_SIZE); // !!!: this is stupid
            AC97_WRITE_BM8(AC97_PO_LVI, target_buffer);
        }
    }

    // FIFO error
    if (status & AC97_SR_FIFO_ERR_INT) {
        LOG(ERR, "FIFO error detected in AC/97 controller\n");
        AC97_WRITE_BM16(AC97_PO_SR, AC97_SR_FIFO_ERR_INT);
    }

    // BUP?
    if (status & AC97_SR_LBE_INT) {
        AC97_WRITE_BM16(AC97_PO_SR, AC97_SR_LBE_INT);
    }

    return 0;
}

/**
 * @brief *Asyncronously* begin playing sound and processing entries in sound_data
 * @param card The card being started
 * @returns 0 on success
 */
int ac97_start(struct sound_card *card) {
    ac97_t *ac = (ac97_t*)card->dev;
    uint8_t cr = (AC97_READ_BM8(AC97_PO_CR)) | (AC97_CR_DMA);
    AC97_WRITE_BM8(AC97_PO_CR, cr);
    return 0;
}

/**
 * @brief Stop the sound card sound
 * @param card The card being stopped
 * @returns 0 on success
 */
int ac97_stop(struct sound_card *card) {
    ac97_t *ac = (ac97_t*)card->dev;
    uint8_t cr = (AC97_READ_BM8(AC97_PO_CR)) & ~(AC97_CR_DMA);
    AC97_WRITE_BM8(AC97_PO_CR, cr);
    return 0;
}

/**
 * @brief Allocate AC/97 BDL
 * @param ac The AC/97 card
 */
void ac97_createBDL(ac97_t *ac) {
    ac->bdl = (ac97_buffer_entry_t*)mem_allocateDMA(sizeof(ac97_buffer_entry_t) * AC97_BDL_ENTRY_COUNT);
    memset(ac->bdl, 0, PAGE_SIZE);

    for (int i = 0; i < AC97_BDL_ENTRY_COUNT; i++) {
        ac->bdl_buffers[i] = (uint32_t*)mem_allocateDMA(AC97_BDL_SIZE+0x1000);
        memset(ac->bdl_buffers[i], 0x00, AC97_BDL_SIZE);

        ac->bdl[i].buffer = (uint32_t)mem_getPhysicalAddress(NULL, (uintptr_t)ac->bdl_buffers[i]);
        ac->bdl[i].samples = AC97_BDL_SAMPLES;
        ac->bdl[i].control = AC97_BDL_CTRL_IOC;
    }

    // Configure BDBAR and LVI
    ac->idx = 2;
    AC97_WRITE_BM32(AC97_PO_BDBAR, mem_getPhysicalAddress(NULL, (uintptr_t)ac->bdl));
    AC97_WRITE_BM8(AC97_PO_LVI, ac->idx);

    LOG(INFO, "BDL list created and allocated to %p (starting idx %d)\n", mem_getPhysicalAddress(NULL, (uintptr_t)ac->bdl), ac->idx);
}

/**
 * @brief Initialize AC/97 controller
 */
int ac97_init(uint32_t address) {
    LOG(INFO, "Found an AC/97 card on bus %d slot %d func %d\n", PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address));
    
    // Create AC97 object
    ac97_t *ac = kzalloc(sizeof(ac97_t));
    ac->pci_device = address;

    // Enable bus mastering and I/O space
    uint16_t command = pci_readConfigOffset(PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address), PCI_COMMAND_OFFSET, 2);
    command |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER;
    pci_writeConfigOffset(PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address), PCI_COMMAND_OFFSET, command, 2);

    // Read I/O base and type
    pci_bar_t *nambbar = pci_readBAR(PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address), 0);
    pci_bar_t *nammbar = pci_readBAR(PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address), 1);
    if (!nambbar || !nammbar) {
        LOG(ERR, "NAMBBAR/NAMMBAR could not be read\n");
        return 1;
    } 

    // Configure
    ac->io_base = (uint32_t)nambbar->address;
    ac->bm_io_base = (uint32_t)nammbar->address;
    ac->io_type = (nammbar->type == PCI_BAR_IO_SPACE);

    if (ac->io_type == 0) {
        ac->io_base = mem_mapMMIO(ac->io_base, nambbar->size);
        ac->bm_io_base = mem_mapMMIO(ac->bm_io_base, nammbar->size);
    }

    LOG(DEBUG, "NAMB Base: %08x NAMM Base: %08x (%s)\n", ac->io_base, ac->bm_io_base, ac->io_type ? "I/O" : "MMIO");

    // Register an IRQ
    // TODO: Support MSI?
    uint8_t irq = pci_getInterrupt(PCI_BUS(address), PCI_SLOT(address), PCI_FUNCTION(address));
    if (irq == 0xFF || hal_registerInterruptHandlerContext(irq, ac97_irq, ac) != 0) {
        LOG(ERR, "AC97 has no IRQ or failed to register it . Cannot continue\n");

        if (ac->io_type == 0) {
            mem_unmapMMIO(ac->io_base, nambbar->size);
            mem_unmapMMIO(ac->bm_io_base, nammbar->size);
        }

        kfree(nambbar);
        kfree(nammbar);
        kfree(ac);
        return 1;
    }

    LOG(DEBUG, "Registered IRQ%d for AC/97 controller\n", irq);

    // Reset everything to default
    // AC97_WRITE16(AC97_REG_RESET, 0xFFFF);

    // Enable IRQs
    AC97_WRITE_BM8(AC97_PO_CR, AC97_CR_FIFO_ERR | AC97_CR_IOC);
    AC97_WRITE16(AC97_REG_PCM_OUTPUT_VOLUME, 0x0000);

    // Create BDL
    ac97_createBDL(ac);

    // Check 5-bit or 6-bit volume support
    // TODO
    AC97_WRITE16(AC97_REG_MASTER_VOLUME, 0x2020);
    uint16_t t = AC97_READ16(AC97_REG_MASTER_VOLUME) & 0x1F;
    if (t == 0x1F) {
        LOG(INFO, "5 bit audio support (0x%x)\n", t);
    } else {
        LOG(INFO, "6 bit audio support (0x%x)\n", t);
    }
    AC97_WRITE16(AC97_REG_MASTER_VOLUME, 0x0000);

    // Make a new sound card object
    sound_card_t *card = sound_createCard("ac97", SOUND_FORMAT_S16PCM, SOUND_RATE_48000HZ);
    card->dev = (void*)ac;
    card->start = ac97_start;
    card->stop = ac97_stop;
    sound_registerCard(card);
    ac->card = card;


    // Enable
    uint8_t cr = (AC97_READ_BM8(AC97_PO_CR) | AC97_CR_DMA);
    AC97_WRITE_BM8(AC97_PO_CR, cr);

    kfree(nambbar);
    kfree(nammbar);
    return 0;
}

/**
 * @brief PCI scan method
 */
int ac97_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    // If this is a match, it must be an AC/97 card
    return ac97_init(PCI_ADDR(bus, slot, function, 0));
}

/**
 * @brief Driver init method
 */
int driver_init(int argc, char *argv[]) {
    return pci_scan(ac97_scan, NULL, 0x0401);
}

/**
 * @brief Driver deinit method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "AC/97 Audio Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};