/**
 * @file hexahedron/include/kernel/drivers/x86/local_apic.h
 * @brief Local APIC header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_X86_LOCAL_APIC_H
#define DRIVERS_X86_LOCAL_APIC_H

/**** INCLUDES ****/
#include <stdint.h>


/**** DEFINITIONS ****/

// Local APIC registers
#define LAPIC_REGISTER_ID           0x020   // ID register
#define LAPIC_REGISTER_VERSION      0x030   // Version register
#define LAPIC_REGISTER_TPR          0x080   // Task priority register
#define LAPIC_REGISTER_APR          0x090   // Arbitrary priority register
#define LAPIC_REGISTER_PPR          0x0A0   // Processor priority register
#define LAPIC_REGISTER_EOI          0x0B0   // EOI register
#define LAPIC_REGISTER_RRD          0x0C0   // Remote read register
#define LAPIC_REGISTER_LOGDEST      0x0D0   // Logical destination register
#define LAPIC_REGISTER_DESTFORMAT   0x0E0   // Destination format register
#define LAPIC_REGISTER_SPURINT      0x0F0   // Spurious interrupt vector register
#define LAPIC_REGISTER_ISR          0x100   // In-service register
#define LAPIC_REGISTER_TMR          0x180   // Trigger mode register
#define LAPIC_REGISTER_IRR          0x200   // Interrupt request register
#define LAPIC_REGISTER_ERROR        0x280   // Error status register
#define LAPIC_REGISTER_CMCI         0x2F0   // LVT corrected machine check interrupt register
#define LAPIC_REGISTER_ICR          0x300   // Interrupt command register
#define LAPIC_REGISTER_TIMER        0x320   // LVT timer register
#define LAPIC_REGISTER_THERMAL      0x330   // LVT thermal sensor register
#define LAPIC_REGISTER_PERF         0x340   // LVT performance monitoring counters register
#define LAPIC_REGISTER_LINT0        0x350   // LVT LINT0 register
#define LAPIC_REGISTER_LINT1        0x360   // LVT LINT1 register
#define LAPIC_REGISTER_LVTERROR     0x370   // LVT error register
#define LAPIC_REGISTER_INITCOUNT    0x380   // Initial count register (for timer)
#define LAPIC_REGISTER_CURCOUNT     0x390   // Current count register (for timer)
#define LAPIC_REGISTER_DIVCONF      0x3E0   // Divide configuration register (for timer)


// EOI
#define LAPIC_EOI                   0x00

// LVT registers
#define LAPIC_LVT_VECTOR            0x000FF // (00-07) Vector number
#define LAPIC_LVT_NMI               0x00700 // (08-10) Reserved for timer, but will be 100B if NMI
#define LAPIC_LVT_POLARITY          0x02000 // (13) Polarity, set is low triggered
#define LAPIC_LVT_REMOTE_IRR        0x04000 // (14) Remote IRR
#define LAPIC_LVT_TRIGGER_MODE      0x08000 // (15) Trigger mode, set is level triggered
#define LAPIC_LVT_SETMASK           0x10000 // (16) Set to mask

// Spurious input vector register
#define LAPIC_SPUR_INTNO            0xFF
#define LAPIC_SPUR_ENABLE           0x100   // Bit 8 is enable

// Interrupt command register (mask)
#define LAPIC_ICR_VECTOR            0x000FF // (0-7) The vector number, or starting page number for SIPIs
#define LAPIC_ICR_DELIVERY          0x00700 // (8-10) The delivery mode. See ICR (delivery)
#define LAPIC_ICR_DESTINATION       0x00800 // (11) The destination mode. See ICR (destination)
#define LAPIC_ICR_SENDING           0x01000 // (12) Delivery status
#define LAPIC_ICR_INITDEASSERT      0x04000 // (15) Set for INIT level deassert
#define LAPIC_ICR_DESTINATIONTYPE   0x60000 // (18-19) Destination type

// Interrupt command register (high)
// The ICR is made of two registers, actually - the one at 0x300 (all of this other stuff pertains to that)
// and one at 0x310, with only one field: local APIC ID
#define LAPIC_ICR_HIGH_ID_SHIFT     24

// Interrupt command register (delivery)
#define LAPIC_ICR_FIXED             0x00000000
#define LAPIC_ICR_LOWEST            0x00000100
#define LAPIC_ICR_SMI               0x00000200
#define LAPIC_ICR_NMI               0x00000400
#define LAPIC_ICR_INIT              0x00000500
#define LAPIC_ICR_STARTUP           0x00000600 

// Interrupt command register (trigger mode)
#define LAPIC_ICR_EDGE              0x0000
#define LAPIC_ICR_LEVEL             0x8000

// Interrupt command register (destination)
#define LAPIC_ICR_DESTINATION_PHYSICAL  0
#define LAPIC_ICR_DESTINATION_LOGICAL   1

// Interrupt command register (destination type)
// NOTE: OSDev wiki recommends only using LAPIC_ICR_DESTINATION_DEFAULT
#define LAPIC_ICR_DESTINATION_DEFAULT       0x0         // Uses the target local APIC ID in the high ICR
#define LAPIC_ICR_DESTINATION_SELF          0x40000     // Send to self
#define LAPIC_ICR_DESTINATION_ALL           0x80000     // Send to all CPUs
#define LAPIC_ICR_DESTINATION_EXCLUDE_SELF  0xC0000     // Send to all processors except self

// Error codes for local APIC (see section 12.5.3 of Intel manuals)
#define LAPIC_SEND_CKSUM_ERROR          0x01
#define LAPIC_RECV_CKSUM_ERROR          0x02
#define LAPIC_SEND_ACCEPT_ERROR         0x04
#define LAPIC_RECV_ACCEPT_ERROR         0x08
#define LAPIC_REDIRECTABLE_IPI          0x10
#define LAPIC_SEND_ILLEGAL_VECTOR       0x20
#define LAPIC_RECV_ILLEGAL_VECTOR       0x40
#define LAPIC_ILLEGAL_REGISTER_ADDRESS  0x80

// Timer IRQ
#define LAPIC_TIMER_IRQ                 0x7B    // ISR 123

/**** FUNCTIONS ****/

/**
 * @brief Initialize the local APIC
 * @param lapic_address Address of MMIO-mapped space of the APIC
 * @returns 0 on success, anything else is failure
 * 
 * @warning PIC must be disabled.
 */
int lapic_initialize(uintptr_t lapic_address);

/**
 * @brief Acknowledge local APIC interrupt
 */
void lapic_acknowledge();

/**
 * @brief Send an NMI to an APIC
 * @param lapic_id The ID of the APIC
 * @param irq_no The IRQ vector to send
 */
void lapic_sendNMI(uint8_t lapic_id, uint8_t irq_no);

/**
 * @brief Send INIT signal
 * @param lapic_id The ID of the APIC
 */
void lapic_sendInit(uint8_t lapic_id);

/**
 * @brief Send SIPI signal
 * @param lapic_id The ID of the APIC
 * @param vector The starting page number (for SIPI - normally vector number)
 */
void lapic_sendStartup(uint8_t lapic_id, uint32_t vector);

/**
 * @brief Enable/disable local APIC via the spurious-interrupt vector register
 * @param enabled Enable/disable
 * @note Register the interrupt handler and the vector yourself
 */
void lapic_setEnabled(int enabled);

/**
 * @brief Get the local APIC version
 */
uint8_t lapic_getVersion();

/**
 * @brief Get the local APIC ID
 */
uint8_t lapic_getID();

/**
 * @brief Gets the current error state of the local apic
 * @returns Error code
 */
uint8_t lapic_readError();

/**
 * @brief Send an IPI to a processor
 * @param lapic_id The ID of the APIC to send to
 * @param irq_no The IRQ vector to send
 * @param flags The flags to use
 */
void lapic_sendIPI(uint8_t lapic_id, uint8_t irq_no, uint32_t flags);

#endif