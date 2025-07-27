/**
 * @file hexahedron/drivers/x86/pic.c
 * @brief Generic-layer (and 8259) PIC system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/x86/io_apic.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>

/* Current selected PIC */
int pic_selected = PIC_TYPE_8259;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:X86:PIC", __VA_ARGS__)

/**** 8259 INTERFACE ****/

uint32_t pic8259_allocations = 0x0; // 16 IRQs

/**
 * @brief Initialize 8259 PIC
 */
static int pic8259_init() {    
    // Begin initialization sequence in cascade mode
    outportb(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4); 
    io_wait();
    outportb(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    // Send offsets
    outportb(PIC1_DATA, 0x20);
    io_wait();
    outportb(PIC2_DATA, 0x28);
    io_wait();

    // Identify slave PIC to be at IRQ2
    outportb(PIC1_DATA, 4);
    io_wait();

    // Notify slave PIC of cascade identity
    outportb(PIC2_DATA, 2);
    io_wait();

    // Switch to 8086 mode
    outportb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outportb(PIC2_DATA, PIC_ICW4_8086);  
    io_wait();

    // Reserve some IRQs (PS/2)
    pic8259_allocations |= (1 << 1) | (1 << 12);

    return 0;
}

/**
 * @brief Stop the 8259 PIC
 */
static void pic8259_shutdown() {
    outportb(PIC1_DATA, 0xFF);
    outportb(PIC2_DATA, 0xFF);
}

/**
 * @brief Mask IRQ in the 8259 PIC
 * @param interrupt The interrupt to mask
 */
static int pic8259_mask(uintptr_t interrupt) {
    uint16_t port = (interrupt < 8 ? PIC1_DATA : PIC2_DATA);
    uint8_t mask = inportb(port) | (1 << ((port == PIC2_DATA) ? interrupt-8 : interrupt));
    outportb(port, mask);

    pic8259_allocations &= ~(1 << interrupt);
    return 0;
}

/**
 * @brief Unmask IRQ in the 8259 PIC
 * @param interrupt The interrupt to unmask
 */
static int pic8259_unmask(uintptr_t interrupt) {
    if (interrupt > 31) return 0;     


    uint8_t irq = (uint8_t)interrupt;
    uint16_t port = (irq < 8 ? PIC1_DATA : PIC2_DATA);
    uint8_t mask = inportb(port) & ~((unsigned)1 << ((port == PIC2_DATA) ? irq-8 : irq));
    outportb(port, mask);
    pic8259_allocations |= (1 << interrupt);
    return 0;
}

/**
 * @brief Send EOI in the 8259 PIC
 * @param interrupt The interrupt to send EOI signal on
 */
static int pic8259_eoi(uintptr_t interrupt) {
    if (interrupt > 8) outportb(PIC2_COMMAND, PIC_8259_EOI);
    outportb(PIC1_COMMAND, PIC_8259_EOI);
    return 0;
}

static uint32_t pic8259_allocate() {
    for (int i = 0; i < 16; i++) {
        if (!(pic8259_allocations & (1 << i))) {
            return i;
        }
    }

    return 0xFFFFFFFF;
}

/**** GENERIC PIC FUNCTIONS ****/

/**
 * @brief Initialize a specific type of PIC
 * @param type The type of PIC to initialize
 * @param data Additional data
 * @returns Whatever the PIC init method does
 */
int pic_init(int type, void *data) {
    switch (type) {
        case PIC_TYPE_8259:
            pic_selected = PIC_TYPE_8259;
            return pic8259_init();
        case PIC_TYPE_IOAPIC:
            int r = ioapic_init(data);
            if (!r) pic_selected = PIC_TYPE_IOAPIC;
            return r;
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }

    return 1;
}

/**
 * @brief Shutdown an old PIC
 * @param type The type of PIC to shutdown
 */
void pic_shutdown(int type) {
    switch (type) {
        case PIC_TYPE_8259:
            return pic8259_shutdown();
        case PIC_TYPE_IOAPIC:
            return ioapic_shutdown();
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }
}

/**
 * @brief Mask an interrupt in the PIC
 * @param interrupt The interrupt to mask
 * @returns 0 on success
 */
int pic_mask(uintptr_t interrupt) {
    switch (pic_selected) {
        case PIC_TYPE_8259:
            return pic8259_mask(interrupt);
        case PIC_TYPE_IOAPIC:
            return ioapic_mask(interrupt);
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }

    return 1;
}

/**
 * @brief Unmask an interrupt in the PIC
 * @param interrupt The interrupt to unmask
 * @returns 0 on success
 */
int pic_unmask(uintptr_t interrupt) {
    switch (pic_selected) {
        case PIC_TYPE_8259:
            return pic8259_unmask(interrupt);
        case PIC_TYPE_IOAPIC:
            return ioapic_unmask(interrupt);
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }

    return 1;
}

/**
 * @brief Send EOI signal to the PIC
 * @param interrupt The interrupt completed
 * @returns 0 on success
 */
int pic_eoi(uintptr_t interrupt) {
    switch (pic_selected) {
        case PIC_TYPE_8259:
            return pic8259_eoi(interrupt);
        case PIC_TYPE_IOAPIC:
            return ioapic_eoi(interrupt);
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }

    return 1;
}

/**
 * @brief Get current PIC type in use
 * @returns The PIC type in use
 */
int pic_type() {
    return pic_selected;
}

/**
 * @brief Allocate an IRQ from the PIC
 * @returns Interrupt or @c 0xFFFFFFFF
 */
uint32_t pic_allocate() {
    switch (pic_selected) {
        case PIC_TYPE_8259:
            return pic8259_allocate();
        case PIC_TYPE_IOAPIC:
            return ioapic_allocate();
        default:
        LOG(ERR, "Unknown PIC type\n");
        break;
    }

    return 0xFFFFFFFF;
}