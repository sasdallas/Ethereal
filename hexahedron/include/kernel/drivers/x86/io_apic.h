/**
 * @file hexahedron/include/kernel/drivers/x86/io_apic.h
 * @brief I/O APIC support
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_X86_IO_APIC_H
#define DRIVERS_X86_IO_APIC_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* IOREGSEL and IOREGWIN */
#define IO_APIC_IOREGSEL                            0x00
#define IO_APIC_IOREGWIN                            0x10

/* Registers (write to IOREGSEL and read from IOREGWIN) */
#define IO_APIC_REG_IOAPICID                        0
#define IO_APIC_REG_IOAPICVER                       1
#define IO_APIC_REG_IOAPICARB                       2
#define IO_APIC_REG_IOREDTBL_BASE                   3

/* Delivery modes */
#define IO_APIC_DELIVERY_MODE_FIXED                 0x00
#define IO_APIC_DELIVERY_MODE_LOWEST_PRIORITY       0x01
#define IO_APIC_DELIVERY_MODE_SMI                   0x02
#define IO_APIC_DELIVERY_MODE_NMI                   0x04
#define IO_APIC_DELIVERY_MODE_INIT                  0x05
#define IO_APIC_DELIVERY_MODE_EXTINT                0x07

/* Destination types */
#define IO_APIC_DESTINATION_MODE_PHYSICAL           0x00
#define IO_APIC_DESTINATION_MODE_LOGICAL            0x01

/* Delivery status */
#define IO_APIC_STATUS_WAITING                      0x00
#define IO_APIC_STATUS_SENT                         0x01

/* Polarity */
#define IO_APIC_POLARITY_ACTIVE_HIGH                0x00
#define IO_APIC_POLARITY_ACTIVE_LOW                 0x01

/* Trigger mode */
#define IO_APIC_TRIGGER_MODE_EDGE                   0x00
#define IO_APIC_TRIGGER_MODE_LEVEL                  0x01

/* Mask */
#define IO_APIC_MASK_OFF                            0x00
#define IO_APIC_MASK_ON                             0x01

/**** TYPES ****/

/**
 * @brief I/O APIC redirection entry
 */
typedef union io_apic_redir_entry {
    struct {   
        uint64_t vector:8;              // Vector
        uint64_t delivery:3;            // Delivery mode
        uint64_t destination_mode:1;    // Destination mode
        uint64_t status:1;              // Delivery status
        uint64_t polarity:1;            // Polarity
        uint64_t remote_irr:1;          // Remote IRR
        uint64_t trigger:1;             // Trigger mode
        uint64_t mask:1;                // Mask
        uint64_t destination:8;         // Destination CPU
    };

    struct {
        uint32_t lo;
        uint32_t hi;
    };

    uint64_t raw;
} io_apic_redir_entry_t;

/**
 * @brief I/O APIC object
 */
typedef struct io_apic {
    uintptr_t mmio_base;                // MMIO base
    uint8_t id;                         // APIC ID
    uint8_t redir_count;                // Redirection count
    uint32_t interrupt_base;            // Interrupt base
} io_apic_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the I/O APIC
 * @param data SMP data
 */
int ioapic_init(void *data);

/**
 * @brief Shutdown the I/O APIC
 */
void ioapic_shutdown();

/**
 * @brief Mask an interrupt in the I/O APIC
 * @param interrupt The interrupt to mask
 */
int ioapic_mask(uintptr_t interrupt);

/**
 * @brief Unmask an interrupt in the I/O APIC
 * @param interrupt The interrupt to unmask
 */
int ioapic_unmask(uintptr_t interrupt);

/**
 * @brief Send EOI to I/O APIC
 * @param interrupt The interrupt to end
 */
int ioapic_eoi(uintptr_t interrupt);

#endif