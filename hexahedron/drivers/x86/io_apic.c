/**
 * @file hexahedron/drivers/x86/io_apic.c
 * @brief I/O APIC driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifdef __ARCH_I386__
#include <kernel/arch/i386/smp.h>
#else
#include <kernel/arch/x86_64/smp.h>
#endif

#include <kernel/arch/arch.h>
#include <kernel/drivers/x86/io_apic.h>
#include <kernel/drivers/x86/local_apic.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* I/O APIC list */
io_apic_t *io_apic_list[MAX_CPUS] = { 0 };

/* I/O APIC amount */
int io_apic_count = 0;

/* IRQ override list */
uint32_t *io_apic_irq_overrides = NULL;

/* Reserved GSI list */
uint8_t reserved_gsis[HAL_IRQ_MSI_COUNT/8] = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:X86:IOAPIC", __VA_ARGS__)

/* Write/read methods for the APIC */
#define IOAPIC_READ(off) (*(uint32_t*)(apic->mmio_base + off))
#define IOAPIC_WRITE(off, value) (*(uint32_t*)(apic->mmio_base + off) = (uint32_t)value)

/**
 * @brief Read register from I/O APIC
 * @param apic The APIC to read from
 * @param reg The register to read from
 */
uint32_t ioapic_read(io_apic_t *apic, uint32_t reg) {
    IOAPIC_WRITE(IO_APIC_IOREGSEL, reg);
    io_wait();
    return IOAPIC_READ(IO_APIC_IOREGWIN);
}

/**
 * @brief Write register to I/O APIC
 * @param apic The APIC to write the register to
 * @param reg The register to write
 * @param value The value to write
 */
void ioapic_write(io_apic_t *apic, uint32_t reg, uint32_t value) {
    IOAPIC_WRITE(IO_APIC_IOREGSEL, reg);
    io_wait();
    IOAPIC_WRITE(IO_APIC_IOREGWIN, value);
}

/**
 * @brief Set a redirection entry in the I/O APIC
 * @param apic The APIC to set the redirection entry in
 * @param pin Pin to set the redirection entry at
 * @param entry The redirection entry to set in the I/O APIC
 * @returns 0 on success
 */
int ioapic_setEntry(io_apic_t *apic, int pin, io_apic_redir_entry_t *entry) {
    // Calculate the offset
    uint32_t reg = 0x10 + (pin * 2);

    // Write entry
    ioapic_write(apic, reg, entry->lo);
    ioapic_write(apic, reg+1, entry->hi);
    return 0;
}

/**
 * @brief Enable an IRQ in an I/O APIC
 * @param irq The IRQ to enable
 * @returns 0 on success
 */
int ioapic_enableIRQ(uintptr_t irq) {
    // Get corresponding GSI in the IRQ overrides
    uint8_t gsi = io_apic_irq_overrides[irq];

    // Find a corresponding I/O APIC
    io_apic_t *apic = NULL;
    for (int i = 0; i < io_apic_count; i++) {
        io_apic_t *apic_i = io_apic_list[i];
        
        if (apic_i->interrupt_base <= gsi && gsi <= apic_i->interrupt_base + apic_i->redir_count) {
            apic = apic_i;
            break;
        }
    }

    if (!apic) {
        LOG(WARN, "Mapping IRQ%d failed: No corresponding APIC was found. (This is probably OK)\n", irq);
        return 0;
    }

    int pin = gsi - apic->interrupt_base;

    LOG(DEBUG, "Mapping an IRQ for Pin %d (GSI: %d, IRQ base: %d)\n", pin, gsi, apic->interrupt_base);

    // Get redirection entry already at that address 
    io_apic_redir_entry_t entry;
    entry.lo = ioapic_read(apic, 0x10 + (pin * 2));
    entry.hi = ioapic_read(apic, 0x10 + (pin * 2 + 1));

    // Setup vector, disable mask, and set to BSP APIC ID
    entry.vector = 32 + irq;
    entry.mask = 0;
    entry.destination = 0;      // TODO: Verify this is actually the BSP ID

    ioapic_setEntry(apic, pin, &entry);

    return 0;
}

/**
 * @brief Initialize the I/O APIC
 * @param data SMP data
 */
int ioapic_init(void *data) {
    smp_info_t *info = (smp_info_t*)data;

    // Collect IRQ overrides
    io_apic_irq_overrides = info->irq_overrides;

    // Start setting up I/O APICs
    for (int i = 0; i < info->ioapic_count; i++) {
        // Create I/O APIC object
        io_apic_t *apic = kzalloc(sizeof(io_apic_t));
        apic->mmio_base = mmio_map(info->ioapic_addrs[i], PAGE_SIZE);
        apic->id = ((ioapic_read(apic, IO_APIC_REG_IOAPICID)) >> 24) & 0xF0;
        apic->redir_count = ((ioapic_read(apic, IO_APIC_REG_IOAPICVER)));
        apic->interrupt_base = info->ioapic_irqbases[i];
        io_apic_list[i] = apic;

        LOG(INFO, "I/O APIC: MMIO=%016llX ID=%02x REDIR=%02x IRQ BASE=%08x\n", apic->mmio_base, apic->id, apic->redir_count, apic->interrupt_base);
        io_apic_count++;
    }
    
    // If we initialized any, continue with init
    if (io_apic_count) {
        // Disable old PIC
        pic_shutdown(PIC_TYPE_8259);
    }
    
    LOG(INFO, "Initialized %d I/O APICs\n", io_apic_count);

    uint32_t gsi = io_apic_irq_overrides[0];
    reserved_gsis[gsi / 8] |= (1 << (gsi % 8));
    gsi = io_apic_irq_overrides[1];
    reserved_gsis[gsi / 8] |= (1 << (gsi % 8));
    gsi = io_apic_irq_overrides[12];
    reserved_gsis[gsi / 8] |= (1 << (gsi % 8));

    return io_apic_count ? 0 : 1;
}

/**
 * @brief Shutdown the I/O APIC
 */
void ioapic_shutdown() {
    // TODO
}

/**
 * @brief Mask an interrupt in the I/O APIC
 * @param interrupt The interrupt to mask
 */
int ioapic_mask(uintptr_t interrupt) {
    uint32_t gsi = io_apic_irq_overrides[interrupt];
    reserved_gsis[gsi / 8] &= ~(1 << (gsi % 8));
    return 0;
}

/**
 * @brief Unmask an interrupt in the I/O APIC
 * @param interrupt The interrupt to unmask
 */
int ioapic_unmask(uintptr_t interrupt) {
    uint32_t gsi = io_apic_irq_overrides[interrupt];
    reserved_gsis[gsi / 8] |= (1 << (gsi % 8));
    return ioapic_enableIRQ(interrupt);
}

/**
 * @brief Send EOI to I/O APIC
 * @param interrupt The interrupt to end
 */
int ioapic_eoi(uintptr_t interrupt) {
    // Forward this to LAPIC
    lapic_acknowledge();
    return 0;
}

/**
 * @brief Allocate an IRQ from the I/O APIC
 */
uint32_t ioapic_allocate() {
    for (int i = 0; i < HAL_IRQ_MSI_BASE - HAL_IRQ_BASE; i++) {
        uint8_t gsi = io_apic_irq_overrides[i];

        // Find the I/O APIC with this GSI
        io_apic_t *apic = NULL;
        for (int i = 0; i < io_apic_count; i++) {
            io_apic_t *apic_i = io_apic_list[i];
            
            if (apic_i->interrupt_base <= gsi && gsi <= apic_i->interrupt_base + apic_i->redir_count) {
                apic = apic_i;
                break;
            }
        }

        if (!apic) {
            continue;
        }

        // Check if this IRQ is reserved
        if (!(reserved_gsis[gsi / 8] & (1 << (gsi % 8)))) {
            LOG(DEBUG, "IRQ%d allocated\n", i);
            reserved_gsis[gsi / 8] |= (1 << (gsi % 8));
            return i;
        }
    }

    return 0xFFFFFFFF;
}