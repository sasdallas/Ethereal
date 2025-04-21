/**
 * @file hexahedron/drivers/x86/pit.c
 * @brief Programmable interval timer driver
 * 
 * @note    This interfaces with the global clock driver, but it functions only as something to update the ticks with.
 *          Because CMOS has no way to be updated every so often, this is done instead.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/x86/pit.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/drivers/clock.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include <kernel/task/process.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>


static volatile uint64_t pit_ticks = 0; // TODO: Make clock_update conform better so we can remove this variable.
static int pit_update = 1;

/**
 * @brief Change the PIT timer phase.
 * @warning Don't touch unless you know what you're doing.
 */
void pit_setTimerPhase(long hz) {
    long divisor = PIT_SCALE / hz; // Only can change the divisor.

    outportb(PIT_MODE, PIT_RATE_GENERATOR | PIT_LOBYTE_HIBYTE);
    outportb(PIT_CHANNEL_A, divisor & 0xFF);
    outportb(PIT_CHANNEL_A, (divisor >> 8) & 0xFF);
}

/**
 * @brief Sleep function
 */
void pit_sleep(uint64_t ms) {
    // !!!: Hacked in method. Doesn't work on some emulators
    ms = ms / 10;
    uint64_t target_ticks = pit_ticks + ms;
    while (target_ticks > pit_ticks) {
        asm volatile ("pause");
    }
}

/**
 * @brief Change the PIT state. 
 * 
 * This is used when the LAPIC timer is initialized, to allow the PIT to still operate as a timer without scheduling
 */
void pit_setState(int enabled) {
    pit_update = enabled;
}

/**
 * @brief IRQ handler
 */
int pit_irqHandler(uintptr_t exception_index, uintptr_t int_number, registers_t *regs, extended_registers_t *regs_extended) {
    pit_ticks++;
    clock_update(clock_readTicks());

    if (!pit_update) return 0; // Done

    // Only if the process is running do we preempt
    if (current_cpu->current_thread && current_cpu->current_process != current_cpu->idle_process && current_cpu->current_thread->status & THREAD_STATUS_RUNNING && !(current_cpu->current_thread->flags & THREAD_FLAG_NO_PREEMPT)) {
        // Is it time to switch processes?
        if (scheduler_update(clock_getTickCount()) == 1) {
            // End interrupt
            hal_endInterrupt(int_number);

            // Yes, it is. Switch to next process
            process_yield(1);
        }
    }

    return 0;
}

/**
 * @brief Initialize PIT
 */
void pit_initialize() {
    // Register handler
    hal_registerInterruptHandler(PIT_IRQ, pit_irqHandler);

    // On default use 100 Hz for divisor
    pit_setTimerPhase(100);

    dprintf_module(INFO, "X86:PIT", "Programmable interval timer initialized\n");
}