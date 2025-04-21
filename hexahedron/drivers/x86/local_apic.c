/**
 * @file hexahedron/drivers/x86/local_apic.c
 * @brief Local APIC driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/x86/local_apic.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/panic.h>
#include <errno.h>

#include <kernel/arch/arch.h>
#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>

#include <kernel/drivers/x86/pit.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/cpu.h>
#include <kernel/arch/i386/registers.h>
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/registers.h>
#include <kernel/arch/x86_64/hal.h>
#else
#error "Unsupported"
#endif

/* APIC base */
uintptr_t lapic_base = 0x0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "X86:LAPIC", __VA_ARGS__)


/**
 * @brief Returns whether the CPU has an APIC
 */
int lapic_available() {
    // Use CPUID
    uint32_t eax, discard, edx;
    __cpuid(CPUID_GETFEATURES, eax, discard, discard, edx);
    return edx & CPUID_FEAT_EDX_APIC;

    return 0;
}

/**
 * @brief Read register from local APIC
 */
uint32_t lapic_read(uint32_t reg) {
    if (!lapic_base) return 0;
    return *((uint32_t volatile*)(lapic_base + reg));
}

/**
 * @brief Write register to local APIC
 * @param reg Register to write
 * @param data Data to write
 */
void lapic_write(uint32_t reg, uint32_t data) {
    if (!lapic_base) return;

    uint32_t volatile *lapic = ((uint32_t volatile*)(lapic_base + reg));
    *lapic = data;
}

/**
 * @brief Get the local APIC ID
 */
uint8_t lapic_getID() {
    uint8_t id = (lapic_read(LAPIC_REGISTER_ID) >> 24) & 0xF;
    return id;
}

/**
 * @brief Get the local APIC version
 */
uint8_t lapic_getVersion() {
    uint32_t ver_reg = lapic_read(LAPIC_REGISTER_VERSION);
    uint8_t lapic_ver = ((ver_reg & 0xFF));
    
    return lapic_ver;
}

/**
 * @brief Enable/disable local APIC via the spurious-interrupt vector register
 * @param enabled Enable/disable
 * @note Register the interrupt handler and the vector yourself
 */
void lapic_setEnabled(int enabled) {
    if (enabled) {
        lapic_write(LAPIC_REGISTER_SPURINT, lapic_read(LAPIC_REGISTER_SPURINT) | LAPIC_SPUR_ENABLE);
    } else {
        lapic_write(LAPIC_REGISTER_SPURINT, lapic_read(LAPIC_REGISTER_SPURINT) & ~LAPIC_SPUR_ENABLE);
    }
}

/**
 * @brief Send an IPI to a processor
 * @param lapic_id The ID of the APIC to send to
 * @param irq_no The IRQ vector to send
 * @param flags The flags to use
 */
void lapic_sendIPI(uint8_t lapic_id, uint8_t irq_no, uint32_t flags) {
    // Write the local APIC ID to the high ICR
    lapic_write(LAPIC_REGISTER_ICR + 0x10, lapic_id << LAPIC_ICR_HIGH_ID_SHIFT);

    // Write the ICR to send the IPI signal
    lapic_write(LAPIC_REGISTER_ICR, flags | irq_no);

    // Wait for send to be completed
    while (lapic_read(LAPIC_REGISTER_ICR) & LAPIC_ICR_SENDING);
}

/**
 * @brief Send SIPI signal
 * @param lapic_id The ID of the APIC
 * @param vector The starting page number (for SIPI - normally vector number)
 */
void lapic_sendStartup(uint8_t lapic_id, uint32_t vector) {
    lapic_sendIPI(lapic_id, 0, (vector / PAGE_SIZE) | LAPIC_ICR_STARTUP | LAPIC_ICR_DESTINATION_PHYSICAL | LAPIC_ICR_INITDEASSERT | LAPIC_ICR_EDGE);
    // LOG(DEBUG, "LAPIC 0x%x SIPI to starting IP 0x%x\n", lapic_id, vector);
}


/**
 * @brief Send an NMI to an APIC
 * @param lapic_id The ID of the APIC
 * @param irq_no The IRQ vector to send
 */
void lapic_sendNMI(uint8_t lapic_id, uint8_t irq_no) {
    lapic_sendIPI(lapic_id, irq_no, LAPIC_ICR_NMI | LAPIC_ICR_DESTINATION_PHYSICAL | LAPIC_ICR_INITDEASSERT | LAPIC_ICR_EDGE);
}

/**
 * @brief Send INIT signal
 * @param lapic_id The ID of the APIC
 */
void lapic_sendInit(uint8_t lapic_id) {
    lapic_sendIPI(lapic_id, 0, LAPIC_ICR_INIT | LAPIC_ICR_DESTINATION_PHYSICAL | LAPIC_ICR_INITDEASSERT | LAPIC_ICR_EDGE);
    // LOG(DEBUG, "LAPIC 0x%x INIT IPI\n", lapic_id);
}

/**
 * @brief Local APIC spurious IRQ
 * @note This is the same between x86_64 and i386.
 */
int lapic_irq(uintptr_t exception_index, uintptr_t irq_number, registers_t *registers, extended_registers_t *extended) {
    LOG(DEBUG, "Spurious local APIC IRQ\n");
    return 0;
}

/**
 * @brief Local APIC timer IRQ
 */
int lapic_timer_irq(uintptr_t exception_index, uintptr_t irq_number, registers_t *registers, extended_registers_t *extended) {
    // Update clock
    clock_update(clock_readTicks());
    
    // Only if the process is running do we preempt
    if (current_cpu->current_thread && current_cpu->current_process != current_cpu->idle_process && current_cpu->current_thread->status & THREAD_STATUS_RUNNING && !(current_cpu->current_thread->flags & THREAD_FLAG_NO_PREEMPT)) {
        // Is it time to switch processes?
        if (scheduler_update(clock_getTickCount()) == 1) {
            // LOG(DEBUG, "Process is out of timeslice - yielding (LAPIC)\n");
            
            // End interrupt
            hal_endInterrupt(irq_number);

            // Yes, it is. Switch to next process
            process_yield(1);
        }
    }

    return 0;
}

/**
 * @brief Acknowledge local APIC interrupt
 */
void lapic_acknowledge() {
    lapic_write(LAPIC_REGISTER_EOI, LAPIC_EOI);
}

/**
 * @brief Gets the current error state of the local apic
 * @returns Error code
 */
uint8_t lapic_readError() {
    // Update ESR
    lapic_write(LAPIC_REGISTER_ERROR, 0);

    // Read ESR
    uint8_t error = lapic_read(LAPIC_REGISTER_ERROR);

    return error;
}


/**
 * @brief Initialize the local APIC
 * @param lapic_address Address of MMIO-mapped space of the APIC
 * @returns 0 on success, anything else is failure
 * 
 * @warning PIC must be disabled.
 */
int lapic_initialize(uintptr_t lapic_address) {
    if (!lapic_available()) {
        LOG(WARN, "No local APIC available\n");
        return -EINVAL;
    }

    // Local APIC base should never change
    if (!lapic_base) {
        // We are BSP
        lapic_base = lapic_address;
    }

    // Register the interrupt handlers
    hal_registerInterruptHandler(LAPIC_SPUR_INTNO - 32, lapic_irq); // NOTE: This might fail occasionally (BSP will reinitialize APICs for each core)
    hal_registerInterruptHandler(LAPIC_TIMER_IRQ - 32, lapic_timer_irq);

    // Disable PIT from updating
    pit_setState(0);

    // Enable spurious vector register
    lapic_write(LAPIC_REGISTER_SPURINT, LAPIC_SPUR_INTNO);
    lapic_setEnabled(1);

    // Register timer IRQ and set divconf
    lapic_write(LAPIC_REGISTER_TIMER, LAPIC_TIMER_IRQ);
    lapic_write(LAPIC_REGISTER_DIVCONF, 1);


    // SOURCE: https://github.com/klange/toaruos/blob/911daaad555e8872a99687121d84197803d9a16c/kernel/arch/x86_64/smp.c
    // Time local APIC against the TSC
    uint64_t before = clock_readTSC();
    lapic_write(LAPIC_REGISTER_INITCOUNT, 1000000);
    while (lapic_read(LAPIC_REGISTER_CURCOUNT));
    uint64_t after = clock_readTSC();

    // Calculate target
    uint64_t ms = (after-before) / clock_getTSCSpeed();
    uint64_t target = 10000000000UL / ms;

    // Setup initial count register
    lapic_write(LAPIC_REGISTER_DIVCONF, 1);
    lapic_write(LAPIC_REGISTER_TIMER, LAPIC_TIMER_IRQ | 0x20000);
    lapic_write(LAPIC_REGISTER_INITCOUNT, target);

    // Enable IRQs in TPR
    lapic_write(LAPIC_REGISTER_TPR, 0);

    // Finished!
    return 0;
}