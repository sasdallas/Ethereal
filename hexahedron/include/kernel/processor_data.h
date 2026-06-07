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
#include <kernel/mm/vmm.h>
#include <kernel/subsystems/clock.h>
#include <kernel/subsystems/timer.h>
#include <kernel/subsystems/irq.h>
#include <kernel/tasklet.h>

/**** TYPES ****/

// Temporary prototypes because this and process file are an include mess
struct process;
struct thread;
struct vmm_context;

typedef struct scheduler_cpu {
    uint8_t state;              // Scheduler state
    list_t *queue;              // Queue data
    spinlock_t *lock;           // Lock
} scheduler_cpu_t;

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
    
    scheduler_cpu_t sched;                  // Scheduler data (POSITION SENSITIVE!)
    
    volatile uint32_t irq_bitmap;           // Per-CPU IRQ bitmap
    irq_table_t *irq_table;                 // IRQ table (FOR PERCPU INTERRUPTS ONLY!!)
    uint64_t idle_time;                     // Time the processor has spent idling
    timekeeper_t timekeeper;                // Timekeeper
    tasklet_cpu_t *tasklet;                 // Tasklet data
    timer_device_t *timer_dev;              // Selected timer device
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
#else
#error "Please define a method of getting processor data"
#endif

/* IRQ bitmap related */

// The IRQ bitmap is taken from Linux, where a 32-bit bitmap tracks different IRQ states.
// PREEMPT is used to track whether the current thread is preemptible (1 = non-preemptible, 0 = preemptible)
// TASKLET is used to track whether the system is currently in a tasklet
// IRQ is used for hardware interrupts
#define PREEMPT_SHIFT      0
#define TASKLET_SHIFT      12
#define IRQ_SHIFT          20
#define PREEMPT_BITS       12
#define TASKLET_BITS       8
#define IRQ_BITS           8
#define PREEMPT_MASK       (((1UL << PREEMPT_BITS) - 1) << PREEMPT_SHIFT)
#define TASKLET_MASK       (((1UL << TASKLET_BITS) - 1) << TASKLET_SHIFT)
#define IRQ_MASK           (((1UL << IRQ_BITS) - 1) << IRQ_SHIFT)

#define INTERRUPT_MASK (IRQ_MASK | TASKLET_MASK)
#define IN_INTERRUPT() ((current_cpu->irq_bitmap & INTERRUPT_MASK) != 0)
#define IN_IRQ() ((current_cpu->irq_bitmap & IRQ_MASK) != 0)
#define IN_TASKLET() ((current_cpu->irq_bitmap & TASKLET_MASK) != 0)
#define IS_PREEMPTIBLE() ((current_cpu->irq_bitmap & PREEMPT_MASK) == 0)
#define IRQ_ENTER() ((current_cpu->irq_bitmap += (1UL << IRQ_SHIFT)))
#define TASKLET_ENTER() ((current_cpu->irq_bitmap += (1UL << TASKLET_SHIFT)))
#define PREEMPT_DISABLE() ((current_cpu->irq_bitmap += (1UL << PREEMPT_SHIFT)))
#define PREEMPT_ENABLE() ((current_cpu->irq_bitmap -= (1UL << PREEMPT_SHIFT)))
#define TASKLET_EXIT() ((current_cpu->irq_bitmap -= (1UL << TASKLET_SHIFT)))
#define IRQ_EXIT() ((current_cpu->irq_bitmap -= (1UL << IRQ_SHIFT)))

#define TASKLET_PENDING() ((current_cpu->tasklet->pending))

#endif