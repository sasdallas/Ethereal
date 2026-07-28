/**
 * @file hexahedron/kernel/smp.c
 * @brief SMP management code for the kernel
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/smp.h>
#include <kernel/debug.h>
#include <kernel/task/process.h>
#include <kernel/misc/mutex.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SMP", __VA_ARGS__)

/* Mask of online CPUs */
procmask_t smp_online_cpus = PROCMASK_INITIALIZER;

/* Processor data */
processor_t processor_data[MAX_CPUS] = { 0 };
int processor_count = 1;

/**
 * @brief Generic AP entry
 */
void smp_apEntry() {
    int cpu = smp_currentCPU();
    LOG(INFO, "Reached generic SMP init for CPU %d\n", cpu);

    // Mark this CPU as online
    procmask_set(&smp_online_cpus, cpu);

    // Prepare idle process for core
    current_cpu->idle_process = process_spawnIdleTask();

    // Jump to scheduler AP initialization (blocks until init phase)
    sched_initAP();
    
    // Switch away
    process_switchNextThread();
}
