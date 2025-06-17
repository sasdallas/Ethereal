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
#include <kernel/arch/arch.h>
#include <kernel/debug.h>

/* Current selected PIC */
int pic_selected = PIC_TYPE_8259;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:X86:PIC", __VA_ARGS__)

/**** 8259 INTERFACE ****/

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
    return 0;
}

/**
 * @brief Unmask IRQ in the 8259 PIC
 * @param interrupt The interrupt to unmask
 */
static int pic8259_unmask(uintptr_t interrupt) {
    uint16_t port = (interrupt < 8 ? PIC1_DATA : PIC2_DATA);
    uint8_t mask = inportb(port) & ~(1 << ((port == PIC2_DATA) ? interrupt-8 : interrupt));
    outportb(port, mask);
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
            LOG(ERR, "I/O APIC support is not implemented yet\n");
            break;
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
            LOG(ERR, "I/O APIC support is not implemented yet\n");
            break;
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
            LOG(ERR, "I/O APIC support is not implemented yet\n");
            break;
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
            LOG(ERR, "I/O APIC support is not implemented yet\n");
            break;
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
            LOG(ERR, "I/O APIC support is not implemented yet\n");
            break;
        default:
            LOG(ERR, "Unknown PIC type\n");
            break;
    }

    return 1;
}