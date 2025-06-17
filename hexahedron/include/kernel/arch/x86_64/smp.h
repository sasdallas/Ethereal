/**
 * @file hexahedron/include/kernel/arch/x86_64/smp.h
 * @brief Symmetric multiprocessing header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_SMP_H
#define KERNEL_ARCH_X86_64_SMP_H

/**** INCLUDES ****/
#include <stdint.h>


/**** DEFINITIONS ****/

// IMPORTANT: This value controls the maximum amount of CPUs Hexahedron supports
#define MAX_CPUS            32

// IMPORTANT: This value controls the maximum amount of supported interrupt overrides
#define MAX_INT_OVERRIDES   0x100

// IMPORTANT: This value controls the page that AP bootstrap code is aligned to (if this isn't an aligned value, I will kill you)
// !!!: DO. NOT. MODIFY. THIS WILL BREAK LITERALLY EVERYTHING!
#define SMP_AP_BOOTSTRAP_PAGE   0x1000

/**** TYPES ****/

// Structure passed to SMP driver containing information (MADT/MP table/whatever) - TODO: check uint32_t?
typedef struct _smp_info {
    // CPUs/local APICs
    void        *lapic_address;             // Local APIC address (will be mmio'd)
    uint8_t     processor_count;            // Amount of processors
    uint8_t     processor_ids[MAX_CPUS];    // Local APIC processor IDs
    uint8_t     lapic_ids[MAX_CPUS];        // Local APIC IDs

    // I/O APICs
    uint16_t    ioapic_count;               // I/O APICs
    uint8_t     ioapic_ids[MAX_CPUS];       // I/O APIC IDs
    uint32_t    ioapic_addrs[MAX_CPUS];     // I/O APIC addresses
    uint32_t    ioapic_irqbases[MAX_CPUS];  // I/O APIC global IRQ bases
    
    // Overrides
    uint32_t    irq_overrides[MAX_INT_OVERRIDES];   // IRQ overrides (index of array = source, content = map)
} smp_info_t;

// AP parameters, unused now mostly
typedef struct _smp_ap_parameters {
    uintptr_t stack;
    uintptr_t idt;
    uintptr_t pagedir;
    uintptr_t lapic_id;
} smp_ap_parameters_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the SMP system
 * @param info Collected SMP information
 * @returns 0 on success, non-zero is failure
 */
int smp_init(smp_info_t *info);

/**
 * @brief Get the amount of CPUs present in the system
 */
int smp_getCPUCount();

/**
 * @brief Get the current CPU's APIC ID
 */
int smp_getCurrentCPU();

/**
 * @brief Shutdown all cores in a system
 * 
 * This causes ISR2 (NMI) to be thrown, disabling the core's interrupts and 
 * looping it on a halt instruction.
 */
void smp_disableCores();

/**
 * @brief Acknowledge core shutdown (called by ISR)
 * 
 * @bug On an NMI, we just assume it's a core shutdown - is this okay?
 */
void smp_acknowledgeCoreShutdown();

/**
 * @brief Perform a TLB shootdown on a specific page
 * @param address The address to perform the TLB shootdown on
 */
void smp_tlbShootdown(uintptr_t address);


#endif