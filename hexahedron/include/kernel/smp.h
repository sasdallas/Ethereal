/**
 * @file hexahedron/include/kernel/smp.h
 * @brief Generic SMP manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_SMP_H
#define KERNEL_SMP_H

/**** INCLUDES ****/
#include <kernel/subsystems/irq.h>
#include <kernel/processor_data.h>
#include <kernel/misc/procmask.h>
#include <stdint.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

/**** MACROS ****/

// There are like 5 of these in the kernel lmao
#define smp_currentCPU() (current_cpu->cpu_id)

/**** VARIABLES ****/

extern procmask_t smp_online_cpus;

/**** FUNCTIONS ****/

/**
 * @brief Generic AP entry
 */
void smp_apEntry();

/**
 * @brief Send IPI to different CPUs
 * @param procmask The processor mask of CPUs to send to
 * @param ipi The IPI vector to send
 * @returns 0 on success
 */
void smp_sendIPI(procmask_t *procmask, irq_number_t ipi);

#endif
