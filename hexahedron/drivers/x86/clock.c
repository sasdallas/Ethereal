/**
 * @file hexahedron/drivers/x86/clock.c
 * @brief x86 clock/CMOS driver
 * 
 * This driver was ported pretty much straight from Ethereal Operating System' original kernel.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include <kernel/drivers/x86/clock.h>
#include <kernel/drivers/x86/pit.h>
#include <kernel/drivers/clock.h>
#include <kernel/debug.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* Log method */
#define LOG(status, format, ...) dprintf_module(status, "CLOCK", format, ## __VA_ARGS__)

/* Architecture boottime (UNIX) */
uint64_t boottime = 0;

/* TSC baseline */
uint64_t tsc_baseline = 0;

/* TSC MHz */
uint64_t tsc_mhz = 0;



static bool is_year_leap(int year) {
    return ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)));
}

static long get_days_in_month(int month, int year) {
    switch (month) {
        case 1:
            return 31; // January
        case 2:
            return is_year_leap(year) ? 29 : 28; // February
        case 3:
            return 31; // March
        case 4:
            return 30; // April
        case 5:
            return 31; // May
        case 6:
            return 30; // June
        case 7:
            return 31; // July
        case 8:
            return 31; // August
        case 9:
            return 30; // September
        case 10:
            return 31; // October
        case 11:
            return 30; // November
        case 12:
            return 31; // December
    }

    return 0; // ??? 
}

// Convert years to seconds
static uint64_t years_to_seconds(int years) {
    uint64_t days = 0;
    if (years < 2000) years += 2000;

    while (years > 1969) {
        days += 365;
        if (is_year_leap(years)) days++;
        years--;
    }

    return days * 86400;
}

// Months to seconds
static uint64_t months_to_seconds(int months, int years) {
    if (years < 2000) years += 2000;

    long days = 0;
    for (int i = 1; i < months; i++) {
        // index starting from 1, scaring all C programmers
        days += get_days_in_month(i, years);
    }

    return days * 86400;
}

/**
 * @brief Returns whether a CMOS update is in progress
 */
int clock_isCMOSUpdateInProgress() {
    outportb(CMOS_ADDRESS, 0x0A);
    return inportb(CMOS_DATA) & 0x80;
}

/**
 * @brief Dump CMOS contents
 * @param values A pointer to a list of uint16_ts of length 128
 */
void clock_dumpCMOS(uint16_t *values) {
    for (uint16_t index = 0; index < 128; index++) {
        outportb(CMOS_ADDRESS, index);
        values[index] = inportb(CMOS_DATA);
    }
}

/**
 * @brief Converts CMOS to a Unix timestamp
 */
uint64_t clock_convertCMOSToUnix() {
    uint16_t values[128];
    uint16_t old_values[128];

    while (clock_isCMOSUpdateInProgress()); // Wait for CMOS
    clock_dumpCMOS((uint16_t*)values);

    do {
        memcpy(old_values, values, 128);
        while (clock_isCMOSUpdateInProgress());
        clock_dumpCMOS((uint16_t*)values);
    } while ((old_values[CMOS_SECOND] != values[CMOS_SECOND]) ||
            (old_values[CMOS_MINUTE] != values[CMOS_MINUTE]) ||
            (old_values[CMOS_HOUR] != values[CMOS_HOUR]) ||
            (old_values[CMOS_DAY] != values[CMOS_DAY]) || 
            (old_values[CMOS_MONTH] != values[CMOS_MONTH]) ||
            (old_values[CMOS_YEAR] != values[CMOS_YEAR]));

    uint64_t time = years_to_seconds(from_bcd(values[CMOS_YEAR]) - 1) +
                    months_to_seconds(from_bcd(values[CMOS_MONTH]),
                    from_bcd(values[CMOS_YEAR])) +
                    (from_bcd(values[CMOS_DAY]) - 1) * 86400 +
                    (from_bcd(values[CMOS_HOUR])) * 3600 +
                    (from_bcd(values[CMOS_MINUTE])) * 60 +
                    from_bcd(values[CMOS_SECOND]) + 0;

    return time;
}

/**
 * @brief Read the current CPU timestamp counter.
 */
uint64_t clock_readTSC() {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi)); // rdtsc outputs lo in EAX and hi in EDX
    return ((uint64_t)hi << 32UL) | (uint64_t)lo;
}

/**
 * @brief Get the TSC speed
 */
size_t clock_getTSCSpeed() {
    return tsc_mhz;
}

/**
 * @brief Gets the tick count (CPU timestamp counter / TSC speed)
 */
uint64_t clock_readTicks() {
    return clock_readTSC() / clock_getTSCSpeed();
}

/**
 * @brief Subdivides tick counts
 */
static void clock_getTickCounts(uint64_t ticks, uint64_t *timer_ticks, uint64_t *timer_subticks) {
    *timer_subticks = ticks - tsc_baseline;
    *timer_ticks = *timer_subticks / SUBSECONDS_PER_SECOND;
    *timer_subticks = *timer_subticks % SUBSECONDS_PER_SECOND;
}

/**
 * @brief Set boottime
 */
static void clock_setBoottime(uint64_t new_boottime) {
    boottime = new_boottime;
}

/**
 * @brief Sleep function
 */
void clock_sleepMilliseconds(uint64_t ms) {
    // !!!: Probably inaccurate and wasteful, but PIT doesn't play well
    struct timeval t;
    clock_gettimeofday(&t, NULL);

    unsigned long target_seconds, target_subseconds;
    clock_relative(ms / 1000, ms & 999, &target_seconds, &target_subseconds);

    target_seconds += boottime; // !!!: Hack

    while (clock_gettimeofday(&t, NULL) == 0) {
        if (t.tv_sec > (time_t)target_seconds) {
            return;
        } else if (t.tv_sec == (time_t)target_seconds) {
            if (t.tv_usec > (suseconds_t)target_subseconds) {
                return;
            }
        }
    }
}

/**
 * @brief Initialize the CMOS-based clock driver.
 */
void clock_initialize() {
    // Calculate the boot time
    boottime = clock_convertCMOSToUnix();

    uintptr_t end_lo, end_hi;
    uint32_t start_lo, start_hi;

    /*
    * I can't even begin to pretend I wrote this code. 
    * It interfaces with the PIT (one-shot mode) and does CPU/TSC calculation.
    * To do this, it modifies the wall clock configuration.
    * All credits to ToaruOS.
    * 
    * Note: It appears that this causes some issues on Bochs, such as incorrect calculation and ear destruction.
    * Who thought it would be a good idea to have the speaker on PIT channel 2?
    * Will investigate further
    */
    asm volatile (
        // Disable PIT gating on channel 2
        "inb $0x61, %%al\n"
        "andb $0xDD, %%al\n"
        "orb $0x01, %%al\n"
        "outb %%al, $0x61\n"

        // Configure channel 2 to one-shot, next two bytes are low/high
        "movb $0xB2, %%al\n" // 10110010
        "outb %%al, $0x43\n"

        // 0x__9b
        "movb $0x9B, %%al\n"
        "outb %%al, $0x42\n"
        "inb $0x60\n"

        // 0x2e__
        "movb $0x2E, %%al\n"
        "outb %%al, $0x42\n"

        // Re-enable
        "inb $0x61, %%al\n"
        "andb $0xDE, %%al\n"
        "outb %%al, $0x61\n"

        // Pulse high
        "orb $0x01, %%al\n"
        "outb %%al, $0x61\n"

        // Read TSC and store it
        "rdtsc\n"
        "movl %%eax, %2\n"
        "movl %%edx, %3\n"

        // QEMU and Virtualbox: This will flip low
        // Real hardware (+ VMWare): This will flip high
        "inb $0x61, %%al\n"
        "andb $0x20, %%al\n"
        "jz 2f\n"
    "1:\n"
        // Loop until output goes low
        "inb $0x61, %%al\n"
        "andb $0x20, %%al\n"
        "jnz 1b\n"
        "rdtsc\n"
        "jmp 3f\n"
    "2:\n"
        // Loop until the output goes high
        "inb $0x61, %%al\n"
        "andb $0x20, %%al\n"
        "jz 2b\n"
        "rdtsc\n"
    "3:\n"
        : "=a"(end_lo), "=d"(end_hi), "=r"(start_lo), "=r"(start_hi)
    );

    // Finalize by doing some bitmask calculation
    uint64_t end = ((uint64_t)(end_hi & 0xFFFFffff) << 32) | (end_lo & 0xFFFFffff);
    uint64_t start = ((uint64_t)(start_hi & 0xFFFFffff) << 32) | (start_lo & 0xFFFFffff);
    tsc_mhz = (end - start) / 10000;

    if (!tsc_mhz) {
        LOG(WARN, "Failed to calculate the TSC MHz - defaulting to 2000\n");
        tsc_mhz = 2000;
    }

    tsc_baseline = start / tsc_mhz;

    LOG(INFO, "TSC calculated speed is %lu MHz\n", tsc_mhz);
    LOG(INFO, "Initial boot time is %lu (UNIX timestamp)\n", boottime);
    LOG(INFO, "TSC baseline is %luus\n", tsc_baseline);

    // Construct and install a clock device
    clock_device_t device;
    device.boot_time = boottime;
    device.get_timer = clock_readTicks; 
    device.get_timer_raw = clock_readTSC;
    device.get_tick_counts = clock_getTickCounts;
    device.set_boottime = clock_setBoottime;
    device.sleep = clock_sleepMilliseconds;

    clock_setDevice(device);
}
