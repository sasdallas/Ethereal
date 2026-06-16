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
 * 
 * @copyright
 * Some portions of this file are under the NCSA License for ToaruOS.
 */

#include <kernel/drivers/x86/local_apic.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/panic.h>
#include <errno.h>

#include <kernel/arch/arch.h>
#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>

#include <kernel/drivers/x86/pit.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/arch/arch.h>

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
inline uint32_t lapic_read(uint32_t reg) {
    if (!lapic_base) return 0;
    return *((uint32_t volatile*)(lapic_base + reg));
}

/**
 * @brief Write register to local APIC
 * @param reg Register to write
 * @param data Data to write
 */
inline void lapic_write(uint32_t reg, uint32_t data) {
    if (!lapic_base) return;
    *((volatile uint32_t*)(lapic_base + reg)) = data;
}

/**
 * @brief Check whether local APIC initialized
 */
inline int lapic_initialized() {
    return !!lapic_base;
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
int lapic_irq(irq_t *irq, void *context) {
    LOG(WARN, "Local APIC spurious interrut\n");
    return IRQ_HANDLED;
}

// reschedule cb
void reschedule_cb(void *ctx) {
    if (current_cpu->current_thread && (current_cpu->current_thread->status & THREAD_STATUS_RUNNING) && !(current_cpu->current_thread->flags & THREAD_FLAG_NO_PREEMPT)) {
        current_cpu->current_thread->flags |= THREAD_FLAG_NEEDS_RESCHED;
    }
}

/**
 * @brief Local APIC timer IRQ
 */
int lapic_timer_irq(irq_t *irq, void *context) {
    // Trigger timer IRQ
    timer_irq();

    return IRQ_HANDLED;
}

/**
 * @brief Acknowledge local APIC interrupt
 */
inline void lapic_acknowledge() {
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
 * @brief Enable local APIC profiling
 */
void lapic_profilingInit() {
    uint32_t lvtpc = lapic_read(LAPIC_REGISTER_PERF);
    lvtpc &= ~(0xff | LAPIC_LVT_SETMASK);
    lvtpc |= 0x400;
    lapic_write(LAPIC_REGISTER_PERF, lvtpc);
}

/**
 * @brief Local APIC timer init
 */
void lapic_timerInit(timer_device_t *device) {
    irq_map(percpu_domain, LAPIC_TIMER_IRQ, LAPIC_TIMER_IRQ, NULL);
    irq_register(LAPIC_TIMER_IRQ, lapic_timer_irq, 0, NULL, NULL);

    // Register timer IRQ
    lapic_write(LAPIC_REGISTER_TIMER, LAPIC_TIMER_IRQ);

    // Time the local APIC using the PIT
    // TODO: clean this up
    lapic_write(LAPIC_REGISTER_DIVCONF, 0x0B); // to divide by 1

    // PIT frequency 1193182 hz
    // Time the lapic by using PIT to sleep for 10ms (freq / 100 = 11932)
    outportb(0x43, 0xb0); // chnl 2 access lo/hi byte mode 3 binary
    outportb(0x42, (uint8_t)((11932 & 0xff))); // low byte
    outportb(0x42, (uint8_t)(((11932 >> 8) & 0xff)));

    // start PIT
    uint8_t sys_ctrl = inportb(0x61);
    sys_ctrl &= 0xFD; sys_ctrl |= 1;
    outportb(0x61, sys_ctrl);

    // configure the local APIC to have its maximum init count
    lapic_write(LAPIC_REGISTER_INITCOUNT, 0xFFFFFFFF);

    // Poll the PIT
    while ((inportb(0x61) & 0x20) == 0) {
        __builtin_ia32_pause();
    }

    outportb(0x61, sys_ctrl & 0xfc);
    uint64_t ticks = 0xFFFFFFFF - lapic_read(LAPIC_REGISTER_CURCOUNT);
    uint64_t tick_per_ns = ((ticks << 32) / 10000000) >> 32;
    LOG(DEBUG, "Calculated %llu ticks per nanosecond (%d ticks pass in 10ms)\n", tick_per_ns, ticks);

    // stop doing that
    lapic_write(LAPIC_REGISTER_INITCOUNT, 0);

    // used for ns -> ticks so calculate the other way
    device->freq.shift = 16;
    uint64_t scaled_hz = (uint64_t)(ticks) << device->freq.shift;
    device->freq.mult = (uint64_t)(scaled_hz / 10000000ULL);
}

/**
 * @brief Local APIC timer deinit
 */
void lapic_timerDeinit(timer_device_t *device) {
    assert(0 && "there is no better timer than me");
}

/**
 * @brief local apic set timer
 */
void lapic_timerSet(timer_device_t *device, uint64_t ticks) {
    assert(ticks < UINT32_MAX);
    lapic_write(LAPIC_REGISTER_INITCOUNT, ticks);
}

/**
 * @brief local apic get ticks elapsed
 */
uint64_t lapic_getTicksElapsed(timer_device_t *device) {
    return (uint32_t)lapic_read(LAPIC_REGISTER_INITCOUNT) - lapic_read(LAPIC_REGISTER_CURCOUNT);
}


/**
 * @brief local apic stop
 */
void lapic_timerStop() {
    lapic_write(LAPIC_REGISTER_INITCOUNT, 0);
}

timer_ops_t lapic_ops = {
    .init = lapic_timerInit,
    .deinit = lapic_timerDeinit,
    .set_timer = lapic_timerSet,
    .stop = lapic_timerStop,
    .get_ticks_elapsed = lapic_getTicksElapsed,
};

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
    irq_map(percpu_domain, LAPIC_SPUR_INTNO, LAPIC_SPUR_INTNO, NULL);
    irq_register(LAPIC_SPUR_INTNO, lapic_irq, 0, NULL, NULL);

    // Enable spurious vector register
    lapic_write(LAPIC_REGISTER_SPURINT, LAPIC_SPUR_INTNO);
    lapic_setEnabled(1);

    // Enable IRQs in TPR
    lapic_write(LAPIC_REGISTER_TPR, 0);

    // Create the timer device
    timer_device_t *lapic_device = timer_createDevice("local apic timer", &lapic_ops, 100, NULL); 
    timer_selectDevice(lapic_device);
    
    // Now that's ready
    timer_event_t *ev = kmalloc(sizeof(timer_event_t));
    timer_init(ev, reschedule_cb, NULL, 10000000, true, "reschedule");
    timer_insert(ev);

    // Finished!
    return 0;
}