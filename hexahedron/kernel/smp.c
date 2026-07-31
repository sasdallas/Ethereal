/**
 * @file hexahedron/kernel/smp.c
 * @brief SMP management code for the kernel
 * 
 * Somehow over and under engineered
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
#include <structs/queue_rb.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SMP", __VA_ARGS__)

/* Mask of online CPUs */
procmask_t smp_online_cpus = PROCMASK_BSP;

/* Processor data */
processor_t processor_data[MAX_CPUS] = { 0 };
int processor_count = 1;

/* Internal IPI */
irq_number_t smp_internal_vector;

/* Internal queues */
typedef struct smp_internal_queue {
    spinlock_t lock;
    void *functions[5];
    void *contexts[5];
    int *completors[5];
    unsigned long ridx;
    unsigned long idx;
    tasklet_t tsklet;
} smp_internal_queue_t;

smp_internal_queue_t smp_queues[MAX_CPUS] = { 0 };

/**
 * @brief Internal tasklet process
 */
static void smp_tasklet(void *context) {
    smp_internal_queue_t *q = context;
    int ran = 0;
    while (1) {
        spinlock_acquire(&q->lock);

        if (q->ridx == q->idx) {
            if (ran == 0) {
                LOG(ERR, "Didn't run anything on CPU%d\n", smp_currentCPU());
            }

            spinlock_release(&q->lock);
            return;
        }

        int slot = q->ridx % 5;

        void (*func)(void *context) = (typeof(func))q->functions[slot];
        void *context = q->contexts[slot];
        int *complete = q->completors[slot];

        q->ridx = (q->ridx + 1);

        spinlock_release(&q->lock);

        func(context);

        if (complete) {
            // LOG(DEBUG, "Finished executing on CPU%d\n", smp_currentCPU());
            __atomic_add_fetch(complete, 1, __ATOMIC_SEQ_CST);
        }

        ran++;
    }
}

/**
 * @brief Internal IPI
 */
int smp_internal(irq_t *irq, void *context) {
    int cpu = smp_currentCPU();
    tasklet_insert(&smp_queues[cpu].tsklet);
    return IRQ_HANDLED;
}

/**
 * @brief Generic AP entry
 */
void smp_apEntry() {
    int cpu = smp_currentCPU();
    LOG(INFO, "Reached generic SMP init for CPU %d\n", cpu);

    // Prepare queue
    SPINLOCK_INIT(&smp_queues[cpu].lock);
    TASKLET_INIT(&smp_queues[cpu].tsklet, "smp internal", smp_tasklet, &smp_queues[cpu]);

    // Register the internal IPI first
    assert(irq_map(percpu_domain, smp_internal_vector, smp_internal_vector, NULL) == 0);
    assert(irq_register(smp_internal_vector, smp_internal, IRQ_FLAG_DEFAULT, NULL, NULL) == 0);

    // Mark this CPU as online
    procmask_set(&smp_online_cpus, cpu);

    // Jump to scheduler AP initialization (blocks until init phase)
    sched_initAP();

    // Prepare idle process for core
    current_cpu->idle_process = process_spawnIdleTask();
    
    // Start scheduler
    sched_start();

    // Switch away
    process_switchNextThread();
}


/**
 * @brief Run function on all CPUs
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunction(void (*func)(void*), void *context, bool wait) {
    return smp_callFunctionMask(&smp_online_cpus, func, context, wait);
}

/**
 * @brief Run function on all other CPUs
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunctionOthers(void (*func)(void*), void *context, bool wait) {
    procmask_t mask = smp_online_cpus;

    int state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    procmask_unset(&mask, current_cpu->cpu_id);
    int rval = smp_callFunctionMask(&mask, func, context, wait);
    hal_setInterruptState(state);
    return rval;
}

/**
 * @brief Run function on CPUs in the mask
 * @param mask The CPU mask
 * @param func The function to run
 * @param context The context to give the function
 * @param wait Whether to wait until completed
 * @returns 0 on success
 */
int smp_callFunctionMask(procmask_t *mask, void (*func)(void*), void *context, bool wait) {
    // Enqueue the function into every CPU specified in mask
    int cpu = procmask_first(mask);
    if (cpu == -1) return 0;

    // Interrupts should be disabled while doing this
    int state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    int completors = 0;
    int cpus = 0;
    while (cpu != -1) {
        smp_internal_queue_t *q = &smp_queues[cpu];
        spinlock_acquire(&q->lock);

        if (q->idx - q->ridx >= 5) {
            assert(0 && "todo block until ready");
        }

        int slot_claimed = q->idx % 5;

        q->completors[slot_claimed] = (wait) ? &completors : NULL;
        q->functions[slot_claimed] = func;
        q->contexts[slot_claimed] = context;

        q->idx += 1;

        spinlock_release(&q->lock);

        cpus += 1;

        cpu = procmask_next(mask, cpu);
    }

    // Figure out delivery method
    int delivery_method = IPI_DEST_CORE;
    if (mask == &smp_online_cpus || !procmask_compare(mask, &smp_online_cpus)) {
        delivery_method = IPI_DEST_ALL;
    } else {
        procmask_t test_mask = smp_online_cpus;
        procmask_unset(&test_mask, smp_currentCPU());
        if (procmask_compare(&test_mask, mask) == 0) {
            delivery_method = IPI_DEST_OTHERS;
        } else if (cpus == 1 && procmask_test(mask, current_cpu->cpu_id)) {
            delivery_method = IPI_DEST_SELF;
        }
    }

    LOG(DEBUG, "callFunctionMask delivery_method=%d\n", delivery_method);

    // Deliver the IPIs 
    if (delivery_method == IPI_DEST_CORE) {
        cpu = procmask_first(mask);
        while (cpu != -1) {
            assert(arch_send_ipi(cpu, smp_internal_vector, IPI_DEST_CORE, IPI_FLAG_DEFAULT) == 0);
            cpu = procmask_next(mask, cpu);
        }
    } else {
        assert(arch_send_ipi(cpu, smp_internal_vector, delivery_method, IPI_FLAG_DEFAULT) == 0);
    }
    
    // Restore interrupts to how they were, because now that we've done that we have waiting to do
    hal_setInterruptState(state);
    
    // TODO make this better
    if (wait) {
        state = hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
        while (__atomic_load_n(&completors, __ATOMIC_SEQ_CST) != cpus) {
            arch_pause_single();
        }
        hal_setInterruptState(state);
    }

    return 0;
}

/**
 * @brief IPI IRQ handler
 */
static int smp_ipiHandler(irq_t *irq, void *context) {
    smp_ipi_t *ipi = context;
    tasklet_insert(&ipi->tasklets[smp_currentCPU()]);
    return IRQ_HANDLED;
}

/**
 * @brief SMP register IPI internal
 */
static void smp_registerIPI(void *context) {
    smp_ipi_t *ipi = context;

    // Can't call irq_map since it can block and allocate memory
    irq_t *irq = ipi->irqs[smp_currentCPU()];
    assert(irq_install(irq, NULL) == 0);
    assert(irq_register(ipi->ipi_vector, smp_ipiHandler, IRQ_FLAG_DEFAULT, ipi, NULL) == 0);
}

/**
 * @brief Allocate and register a new IPI
 * @param name Optional name for the IPI
 * @param callback The callback to call when the IPI is sent
 * @param context Context for the callback
 * @returns Global IPI structure
 */
smp_ipi_t *smp_createIPI(char *name, void (*callback)(void*), void *context) {
    smp_ipi_t *ipi = kmalloc(sizeof(smp_ipi_t));

    // !!! This is a very, very silly solution, don't get me wrong. However.. it does work.
    ipi->tasklets = kmalloc(sizeof(tasklet_t) * processor_count);
    for (int i = 0; i < processor_count; i++) {
        TASKLET_INIT(&ipi->tasklets[i], name, callback, context);
    }

    assert(irq_allocate(percpu_domain, 0, NULL, &ipi->ipi_vector) == 0);
    assert(irq_register(ipi->ipi_vector, smp_ipiHandler, IRQ_FLAG_DEFAULT, ipi, NULL) == 0);

    // !!! This is ridiculous. The cores that need the IRQ objects can't allocate within tasklet context
    // !!! so we have to preallocate for them.
    ipi->irqs = kmalloc(sizeof(irq_t*) * processor_count);
    for (int i = 0; i < processor_count; i++) {
        ipi->irqs[i] = irq_create(percpu_domain, ipi->ipi_vector, 0);
    }

    smp_callFunctionOthers(smp_registerIPI, ipi, true);

    LOG(INFO, "Created new SMP IPI \"%s\" at vector %d\n", name, ipi->ipi_vector);

    return ipi;
}

/**
 * @brief Send IPI to all other CPUs
 * @param ipi The IPI to send
 */
void smp_sendOthersIPI(smp_ipi_t *ipi) {
    arch_send_ipi(0, ipi->ipi_vector, IPI_DEST_OTHERS, IPI_FLAG_DEFAULT);
}

/**
 * @brief Send IPI to specific CPU
 * @param cpu The CPU to send the IPI to
 * @param ipi The IPI to send
 */
void smp_sendCoreIPI(int cpu, smp_ipi_t *ipi) {
    arch_send_ipi(cpu, ipi->ipi_vector, IPI_DEST_CORE, IPI_FLAG_DEFAULT);
}

/**
 * @brief Prepare internal SMP structures
 */
void smp_genericInit() {
    memset(smp_queues, 0, sizeof(smp_queues));

    // Prepare BSP queue
    SPINLOCK_INIT(&smp_queues[0].lock);
    TASKLET_INIT(&smp_queues[0].tsklet, "smp internal", smp_tasklet, NULL);

    // Allocate the internal IPI vector on the BSP
    assert(irq_allocate(percpu_domain, 0, NULL, &smp_internal_vector) == 0);
    assert(irq_register(smp_internal_vector, smp_internal, IRQ_FLAG_DEFAULT, NULL, NULL) == 0);
}
