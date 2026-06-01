/**
 * @file hexahedron/drivers/x86/irq.c
 * @brief IRQ domains for x86
 * 
 * Implements a few domains:
 * - Global domain for hardware interrupts (global_domain)
 * - Per-CPU domain for per-CPU interrupts (percpu_domain) 
 * - MSI domain for message-signalled interrupts (msi_domain)
 * - MSI-X domain (msix_domain)
 * 
 * Why have per-CPU domain?
 * Because per-CPU interrupts need to still be EOI'd by the local APIC
 * but can't be unmasked in the I/O APIC.
 * 
 * Why have MSI/MSI-X domain be arch-specific?
 * Because by definition MSI and MSI-X are arch-specific. You have to write the local
 * APIC address to the PCI device fields.
 * 
 * Why make this IRQ subsystem so complicated?
 * To prevent IRQ conflicts. This way, with domain separation they are impossible.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/subsystems/irq.h>
#include <kernel/drivers/x86/local_apic.h>
#include <kernel/drivers/x86/io_apic.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/pci.h>
#include <kernel/debug.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "IRQ:X86", __VA_ARGS__)

/* Local APIC */
static void lapic_chipMask(irq_t *irq);
static void lapic_chipUnmask(irq_t *irq);
static void lapic_chipSetAffinity(irq_t *irq, procmask_t pmask);
static void lapic_chipEoi(irq_t *irq);
irq_chip_t __lapic_chip = {
    .name = "Local APIC",
    .ops = {
        .irq_mask = lapic_chipMask,
        .irq_unmask = lapic_chipUnmask,
        .irq_set_affinity = lapic_chipSetAffinity,
        .irq_eoi = lapic_chipEoi,
    },
    .priv = NULL,
};

/* Global PIC */
static void pic_chipMask(irq_t *irq);
static void pic_chipUnmask(irq_t *irq);
static void pic_chipSetAffinity(irq_t *irq, procmask_t mask);
static void pic_chipEoi(irq_t *irq);
irq_chip_t __pic_chip = {
    .name = "Global PIC",
    .ops = {
        .irq_mask = pic_chipMask,
        .irq_unmask = pic_chipUnmask,
        .irq_set_affinity = pic_chipSetAffinity,
        .irq_eoi = pic_chipEoi
    },
    .priv = NULL
};

/* Per-CPU */
irq_domain_t __percpu_internal = {
    .domain_name = "percpu",
    .chip = &__lapic_chip,
    .ops = {
        .domain_map = NULL,
        .domain_alloc = NULL,
    },
    .priv = NULL,
};

/* Global */
static int global_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev);
static int global_alloc(irq_domain_t *domain, int hwirq, void *dev, irq_number_t *ret);
irq_domain_t __global_internal = {
    .domain_name = "global",
    .chip = &__pic_chip,
    .ops = {
        .domain_map = global_map,
        .domain_alloc = global_alloc,
    },
    .priv = NULL
};

/* MSI */
static int msi_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev);
irq_domain_t __msi_internal = {
    .domain_name = "msi",
    .chip = &__lapic_chip,
    .ops = {
        .domain_map = msi_map,
        .domain_alloc = NULL
    },
    .priv = NULL
};

/* MSI-X */
static int msix_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev);
irq_domain_t __msix_internal = {
    .domain_name = "msix",
    .chip = &__lapic_chip,
    .ops = {
        .domain_map = msix_map,
        .domain_alloc = NULL
    }
};

/* All domains */
irq_domain_t *percpu_domain = &__percpu_internal;
irq_domain_t *global_domain = &__global_internal;
irq_domain_t *msi_domain = &__msi_internal;
irq_domain_t *msix_domain = &__msix_internal;

/* Local APIC callbacks */

static void lapic_chipMask(irq_t *irq) {
    return; // TODO this can actually be done on a few
}

static void lapic_chipUnmask(irq_t *irq) {
    return; // TODO this can actually be done on a few
}

static void lapic_chipSetAffinity(irq_t *irq, procmask_t pmask) {
    // ummm... not really how that works..
    LOG(WARN, "The fuck are you trying to do?\n");
}

static void lapic_chipEoi(irq_t *irq) {
extern uintptr_t lapic_base;
    *((volatile uint32_t*)(lapic_base + LAPIC_REGISTER_EOI)) = LAPIC_EOI;
}

/* PIC callbacks */

static void pic_chipMask(irq_t *irq) {
    pic_mask(irq->hwirq);
}

static void pic_chipUnmask(irq_t *irq) {
    pic_unmask(irq->hwirq);
}

static void pic_chipSetAffinity(irq_t *irq, procmask_t mask) {
    LOG(WARN, "PIC IRQ set affinity not implemented yet\n");
}

static void pic_chipEoi(irq_t *irq) {
    // TODO: this takes a while
    pic_eoi(irq->hwirq);
}

/* Global map and alloc */
static int global_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev) {
    return pic_route(irq->num, hwirq);
}

static int global_alloc(irq_domain_t *domain, int hwirq, void *dev, irq_number_t *ret) {
    if (pic_type() == PIC_TYPE_8259) {
        // 8259 PIC does not support IRQ redirections. Must be identity mapped
        *ret = (irq_number_t)(hwirq + HAL_IRQ_BASE);
        return 0;
    }

    // Otherwise just allocate
    *ret = irq_allocateVector();
    return 0;
}

/* MSI */
static int msi_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev) {
    pci_device_t *pcidev = (void*)dev;

    // read PCI message control
    uint16_t ctrl;
    pci_readConfigWord(pcidev, pcidev->msi_offset + 0x02, &ctrl);

    if (ctrl & (1 << 7)) {
        // 64-bit supported
        // !!!! HACK GET ACTUAL APIC BASE
        uint64_t addr = 0xFEE00000;
        uint16_t data = irq->num & 0xFFFF;
        pci_writeConfigDword(pcidev, pcidev->msi_offset + 0x04, (addr & 0xFFFFFFFF));
        pci_writeConfigDword(pcidev, pcidev->msi_offset + 0x08, ((addr >> 32) & 0xFFFFFFFF));
        pci_writeConfigWord(pcidev, pcidev->msi_offset + 0x0C, data);
    } else {
        // 64-bit not supported
        // !!!! HACK GET ACTUAL APIC BASE
        uint32_t addr = 0xFEE00000;
        uint16_t data = irq->num & 0xFFFF;
        pci_writeConfigDword(pcidev, pcidev->msi_offset + 0x04, (addr & 0xFFFFFFFF));
        pci_writeConfigWord(pcidev, pcidev->msi_offset + 0x08, data);
    }

    return 0;
}

/* MSI-X */
/* TODO: Implement MSI-X chip for masking/unmasking */
static int msix_map(irq_domain_t *domain, irq_t *irq, int hwirq, void *dev) {
    // dev points to the start of the table in memory
    // hwirq is the index within the table
    pci_msix_entry_t *entry = &((pci_msix_entry_t*)dev)[hwirq];
    
    // !!! HACK GET ACTUAL APIC BASE
    uint64_t addr = 0xFEE00000;
    uint32_t data = irq->num;
    entry->msg_addr_low = (uint32_t)(addr & 0xFFFFFFFF);
    entry->msg_addr_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    entry->msg_data = data;
    entry->vector_ctrl = entry->vector_ctrl & ~1u; // clears masked bit

    return 0;
}
