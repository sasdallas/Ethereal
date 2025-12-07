/**
 * @file hexahedron/kernel/init.c
 * @brief Replica of the Linux initcall system
 * 
 * @see init.h for initcall explanation
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/init.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/drivers/clock.h>
#include <sys/time.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "INIT", __VA_ARGS__)

/* UNCOMMENT TO ENABLE DEBUG */
#define INIT_DEBUG 1

/* init phase */
struct init_phase {
    char *name;
    kernel_initcall_t **start;
    kernel_initcall_t **stop;
};

#define __DEFINE_INIT_PHASE2(p) \
    extern kernel_initcall_t **__initcall_ ## p ##  _start; \
    extern kernel_initcall_t **__initcall_ ## p ## _end; \

#define __DEFINE_INIT_PHASE(p) __DEFINE_INIT_PHASE2(p)

/* INIT PHASE DEFINITIONS */
__DEFINE_INIT_PHASE(PHASE_KERN_EARLY);
__DEFINE_INIT_PHASE(PHASE_FS);
__DEFINE_INIT_PHASE(PHASE_NET);
__DEFINE_INIT_PHASE(PHASE_SCHED);
__DEFINE_INIT_PHASE(PHASE_ROOTFS);
__DEFINE_INIT_PHASE(PHASE_DRIVER);
__DEFINE_INIT_PHASE(PHASE_KERN_LATE);


#define __INIT_PHASE2(p) \
    { .name = #p, .start = (kernel_initcall_t**)&__initcall_## p ##_start, .stop = (kernel_initcall_t**)&__initcall_## p ## _end }

#define __INIT_PHASE(p) __INIT_PHASE2(p)

static struct init_phase init_phases[] = {
    __INIT_PHASE(PHASE_KERN_EARLY),
    __INIT_PHASE(PHASE_FS),
    __INIT_PHASE(PHASE_NET),
    __INIT_PHASE(PHASE_SCHED),
    __INIT_PHASE(PHASE_ROOTFS),
    __INIT_PHASE(PHASE_DRIVER),
    __INIT_PHASE(PHASE_KERN_LATE),
};

/**
 * @brief Get init phase
 */
static int init_getPhase(const char *phase, kernel_initcall_t ***start, kernel_initcall_t ***stop) {
    for (unsigned i = 0; i < sizeof(init_phases) / sizeof(struct init_phase); i++) {
        if (!strcmp(init_phases[i].name, phase)) {
            *start = init_phases[i].start;
            *stop = init_phases[i].stop;
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Find init dependency
 */
static kernel_initcall_t *__init_find_dependency(kernel_initcall_t **start, size_t size, const char *dep) {
    for (unsigned i = 0; i < size; i++) {
        if (!strcmp(start[i]->name, dep)) {
            return start[i];
        }
    }

    return NULL;
}


/**
 * @brief Execute initcall
 */
static void __init_run_initcall(kernel_initcall_t **start, size_t list_size, kernel_initcall_t *call, kernel_initcall_t *parent) {
    if (call->flags == 0xffffffff) {
        return; // routine execution completed
    }

    const char **dep = call->deps;
    while (*dep) {    
        // Locate and run dependency
        kernel_initcall_t *dep_call = __init_find_dependency(start, list_size, *dep);
        if (!dep_call) {
            if ((call->flags & INIT_FLAG_IGNORE_MISSING_DEPS) == 0) {
                kernel_panic_extended(MISSING_INIT_DEPENDENCY, "initcall", "*** Init routine '%s' is missing dependency '%s'\n", call->name, *dep);
            }

            continue;
        }

        __init_run_initcall(start, list_size, dep_call, call);

        dep++;
    }

    // All dependencies executed, run routine now
    LOG(INFO, "Running init routine '%s'...\n", call->name);

    int ret = call->function(parent);
    if (ret != 0) {
        LOG(ERR, "Init routine '%s' failed.\n", call->name);
        if ((call->flags & INIT_FLAG_CAN_FAIL) == 0) {
            kernel_panic_extended(INIT_ROUTINE_FAILURE, "initcall", "*** Init routine '%s' returned status code %d\n", call->name, ret);
        }
    }

    if ((call->flags & INIT_FLAG_RUN_MULTIPLE) == 0) {
        call->flags = 0xffffffff;
    }
}

/**
 * @brief Execute an initcall phase
 */
void __init_run_phase(const char *phase) {
    // Figure out the phase
    kernel_initcall_t **start;
    kernel_initcall_t **stop;
    if (init_getPhase(phase, &start, &stop) != 0) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "initcall", "*** Bad init phase %s\n", phase);
        __builtin_unreachable();
    }


    size_t init_count = stop - start;
#ifdef INIT_DEBUG
    LOG(INFO, "========= Running initialization phase \"%s\" (%d modules)\n", phase, init_count);

    struct timeval tv;
    gettimeofday(&tv, NULL);
#endif

    for (unsigned i = 0; i < init_count; i++) {
        __init_run_initcall(start, init_count, start[i], NULL);
    }


#ifdef INIT_DEBUG
    struct timeval tv_fin;
    struct timeval tv_sub;
    clock_gettimeofday(&tv_fin, NULL);
    
    // taken from mlibc
    tv_sub.tv_sec = tv_fin.tv_sec - tv.tv_sec;
    tv_sub.tv_usec = tv_fin.tv_usec - tv.tv_usec;
    while(tv_sub.tv_usec < 0) {
		tv_sub.tv_usec += 1000000;
		tv_sub.tv_sec -= 1;
	}

    LOG(INFO, "========= Initialization phase \"%s\" finished in %d.%06d\n", phase, tv_sub.tv_sec, tv_sub.tv_usec);
#endif
}

