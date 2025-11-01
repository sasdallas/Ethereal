/**
 * @file hexahedron/include/kernel/processor_data.h
 * @brief Implements a generic and architecture-specific CPU core data system
 * 
 * The architectures that implement SMP need to update this structure with their own 
 * data fields. Generic data fields (like the current process, cpu_id, etc.) are required as well. 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_PROCESSOR_DATA_H
#define KERNEL_PROCESSOR_DATA_H

#pragma GCC diagnostic ignored "-Wunused-variable"

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/mem/mem.h>
#include <kernel/task/process.h>
#include <kernel/mm/vmm.h>

/**** TYPES ****/

// Temporary prototypes because this and process file are an include mess
struct process;
struct thread;
struct vmm_context;

typedef struct _processor {
    int cpu_id;                             // CPU ID
    struct vmm_context *current_context;    // Current page directory
    struct thread *current_thread;          // Current thread of the process

    struct process *current_process;        // Current process of the CPU
    struct process *idle_process;           // Idle process of the CPU

#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)

#ifdef __ARCH_X86_64__
    uintptr_t kstack;                       // (0x40) Kernel-mode stack loaded in TSS
    uintptr_t ustack;                       // (0x48) Usermode stack, saved in SYSCALL entrypoint    
#endif

    int lapic_id;

    /* CPU basic information */
    char cpu_model[48];
    const char *cpu_manufacturer;
    int cpu_model_number;
    int cpu_family;
#endif
    
    scheduler_cpu_t sched;                  // Scheduler data
    uint64_t idle_time;                     // Time the processor has spent idling
} processor_t;

/* External variables defined by architecture */
extern processor_t processor_data[];
extern int processor_count;

/**
 * @brief Architecture-specific method of determining current core
 * 
 * i386: We use a macro that retrieves the current data from processor_data
 * x86_64: We use the GSbase to get it
 */

#if defined(__ARCH_I386__) || defined(__INTELLISENSE__)

#define current_cpu ((processor_t*)&(processor_data[arch_current_cpu()]))

#elif defined(__ARCH_X86_64__)

static processor_t __seg_gs * const current_cpu = 0;

#elif defined(__ARCH_AARCH64__)

register processor_t * current_cpu asm("x18"); 

#else
#error "Please define a method of getting processor data"
#endif


#endif