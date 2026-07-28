/**
 * @file hexahedron/task/sched/sched.c
 * @brief Primary scheduler interface for Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/sched/sched.h>
#include <kernel/task/process.h>
#include <kernel/misc/args.h>
#include <kernel/config.h>
#include <kernel/panic.h>
#include <kernel/debug.h>

/* Schedulers */
extern sched_t dumb_scheduler;

static sched_t *schedulers[] = {
    &dumb_scheduler
};

/* Current scheduler */
sched_t *sched_current = NULL;

/* Hack for APs */
static bool sched_ap_ready = false;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SCHED", __VA_ARGS__)

/**
 * @brief Initialize the scheduler for the BSP
 */
void sched_init() {
    // Pick a scheduler
    char *target_scheduler = KERNEL_DEFAULT_SCHEDULER;
    if (kargs_has("--sched")) {
        char *arg_sched = kargs_get("--sched");
        if (arg_sched != NULL) {
            target_scheduler = arg_sched;
        }
    }

    sched_t *scheduler = NULL;
    for (unsigned i = 0; i < sizeof(schedulers)/sizeof(schedulers[0]); i++) {
        if (strcasecmp(target_scheduler, schedulers[i]->name) == 0) {
            // Located
            scheduler = schedulers[i];
            break;
        }
    }

    if (scheduler == NULL) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "sched", "*** Could not find scheduler \"%s\"\n", target_scheduler);
    }

    // Initialize the scheduler
    sched_current = scheduler;
    sched_current->ops.sched_init();
    LOG(DEBUG, "Initialized scheduler \"%s\"\n", target_scheduler);

    // Ready for the APs now
    // TODO send the cores an IPI they will wake up eventually probably
    __atomic_store_n(&sched_ap_ready, true, __ATOMIC_SEQ_CST);
}


/**
 * @brief Initialize the scheduler for a sub-processor
 */
void sched_initAP() {
    // !!! Hack because this kernel init sequence is weird, the scheduler only inits after the cores are done initting
    while (__atomic_load_n(&sched_ap_ready, __ATOMIC_SEQ_CST) == false) {
        arch_pause();
    }

    sched_current->ops.sched_ap();
}
