/**
 * @file hexahedron/include/kernel/drivers/x86/pic.h
 * @brief Generic-layer (and 8259) PIC driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_X86_PIC_H
#define DRIVERS_X86_PIC_H

/**** INCLUDES ****/

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "Unknown architecture"
#endif

/**** DEFINITIONS ****/

// PIC types
#define PIC_TYPE_8259   0
#define PIC_TYPE_IOAPIC 1

// PIC definitions
#define PIC1_ADDR       0x20                // Master PIC address
#define PIC2_ADDR       0xA0                // Slave PIC address
#define PIC1_COMMAND    (PIC1_ADDR)         // Command address for master PIC
#define PIC2_COMMAND    (PIC2_ADDR)         // Command address for slave PIC
#define PIC1_DATA       (PIC1_ADDR)+1       // Data address for master PIC
#define PIC2_DATA       (PIC2_ADDR)+1       // Data address for slave PIC

#define PIC_8259_EOI        0x20            // End of interrupt code

// PIC ICW (initialization words)
#define PIC_ICW1_ICW4       0x01            // Indicates that ICW4 is present
#define PIC_ICW1_SINGLE     0x02            // Enables PIC cascade mode
#define PIC_ICW1_INTERVAL4  0x04            // Call address interval 4
#define PIC_ICW1_LEVEL      0x08            // Edge mode
#define PIC_ICW1_INIT       0x10            // Initialization bit

#define PIC_ICW4_8086       0x01            // Enables 8086/88 mode
#define PIC_ICW4_AUTO       0x02            // Auto EOI 
#define PIC_ICW4_BUF_SLAVE  0x08            // Enable buffered mode (slave)
#define PIC_ICW4_BUF_MASTER 0x0C            // Enable buffered mode (master)
#define PIC_ICW4_SFNM       0x10            // Special fully nested

/**** FUNCTIONS ****/

/**
 * @brief Initialize a specific type of PIC
 * @param type The type of PIC to initialize
 * @param data Additional data
 * @returns Whatever the PIC init method does
 */
int pic_init(int type, void *data);

/**
 * @brief Shutdown an old PIC
 * @param type The type of PIC to shutdown
 */
void pic_shutdown(int type);

/**
 * @brief Mask an interrupt in the PIC
 * @param interrupt The interrupt to mask
 * @returns 0 on success
 */
int pic_mask(uintptr_t interrupt);

/**
 * @brief Unmask an interrupt in the PIC
 * @param interrupt The interrupt to unmask
 * @returns 0 on success
 */
int pic_unmask(uintptr_t interrupt);

/**
 * @brief Send EOI signal to the PIC
 * @param interrupt The interrupt completed
 * @returns 0 on success
 */
int pic_eoi(uintptr_t interrupt);

/**
 * @brief Get current PIC type in use
 * @returns The PIC type in use
 */
int pic_type();

#endif