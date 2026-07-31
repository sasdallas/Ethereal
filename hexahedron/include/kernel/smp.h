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

/* Flags for the IPI */
#define IPI_FLAG_DEFAULT            0x0
#define IPI_FLAG_NMI                0x1

/* Destination */
#define IPI_DEST_CORE               0
#define IPI_DEST_SELF               1
#define IPI_DEST_ALL                2
#define IPI_DEST_OTHERS             3

/**** TYPES ****/

typedef struct smp_ipi {
    irq_t **irqs;
    tasklet_t *tasklets;
    irq_number_t ipi_vector;
} smp_ipi_t;

/**** MACROS ****/

// There are like 5 of these in the kernel lmao
#define smp_currentCPU() (current_cpu->cpu_id)

/* Whether this is the boot processor */
#define IS_BSP() (current_cpu->cpu_id == 0)

/**** VARIABLES ****/

extern procmask_t smp_online_cpus;

/**** FUNCTIONS ****/

/**
 * @brief Generic AP entry
 */
void smp_apEntry();

/**
 * @brief Run function on all CPUs
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunction(void (*func)(void*), void *context, bool wait);

/**
 * @brief Run function on all other CPUs
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunctionOthers(void (*func)(void*), void *context, bool wait);

/**
 * @brief Run function on CPUs in the mask
 * @param mask The CPU mask
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunctionMask(procmask_t *mask, void (*func)(void*), void *context, bool wait);

/**
 * @brief Allocate and register a new IPI
 * @param name Optional name for the IPI
 * @param callback The callback to call when the IPI is sent
 * @param context Context for the callback
 * @returns Global IPI structure
 */
smp_ipi_t *smp_createIPI(char *name, void (*callback)(void*), void *context);

/**
 * @brief Send IPI to all other CPUs
 * @param ipi The IPI to send
 */
void smp_sendOthersIPI(smp_ipi_t *ipi);

/**
 * @brief Send IPI to specific CPU
 * @param cpu The CPU to send the IPI to
 * @param ipi The IPI to send
 */
void smp_sendCoreIPI(int cpu, smp_ipi_t *ipi);

/**
 * @brief Prepare internal SMP structures
 */
void smp_genericInit();

#endif
