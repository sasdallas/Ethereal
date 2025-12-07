/**
 * @file hexahedron/include/kernel/init.h
 * @brief initcall system
 * 
 * The initcall system of Hexahedron operates on a combination of the Linux design and my own garbage.
 * You will provide a phase and any additional dependencies (from within your phase)
 * 
 * kmain is called when HAL has finished doing some of its basic initialization
 * this includes early logging, ACPI (x86), clock system, PCI, video, etc.
 * 
 * Then the phases are so:
 * KERN_EARLY -> FS -> SCHED -> ROOTFS -> DRIVER -> KERN_LATE  
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

/**** INCLUDES ****/
#include <stddef.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define PHASE_KERN_EARLY        kern_early
#define PHASE_FS                fs
#define PHASE_NET               net
#define PHASE_SCHED             sched
#define PHASE_ROOTFS            rootfs
#define PHASE_DRIVER            driver
#define PHASE_KERN_LATE         kern_late

#define INIT_FLAG_DEFAULT               0x0
#define INIT_FLAG_CAN_FAIL              0x1
#define INIT_FLAG_RUN_MULTIPLE          0x2
#define INIT_FLAG_IGNORE_MISSING_DEPS   0x4 // idk why anyone would use this, but...

/**** TYPES ****/

struct kernel_initcall;

/* "parent" is the top dependency. optional parameter. */
typedef int (*kernel_initcall_fn_t)(struct kernel_initcall *parent);

typedef struct kernel_initcall {
    const char *name;
    const char **deps;
    kernel_initcall_fn_t function;
    uint32_t flags; // set to 0xffffffff when finished running, if INIT_FLAG_RUN_MULTIPLE not set.
} kernel_initcall_t;

/**** MACROS ****/

/* silly macros */
#define __HAS_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define __HAS_ARGS(...)  __HAS_ARGS_IMPL(0, ## __VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define __INIT_CONCAT2(a, b) a##b
#define __INIT_CONCAT(a, b) __INIT_CONCAT2(a,b)
#define __INIT_STRINGIFY(x) #x

#define __INIT_DEP_0(d, ...) NULL
#define __INIT_DEP_1(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_0(__VA_ARGS__)
#define __INIT_DEP_2(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_1(__VA_ARGS__)
#define __INIT_DEP_3(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_2(__VA_ARGS__)
#define __INIT_DEP_4(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_3(__VA_ARGS__)
#define __INIT_DEP_5(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_4(__VA_ARGS__)
#define __INIT_DEP_6(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_5(__VA_ARGS__)
#define __INIT_DEP_7(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_6(__VA_ARGS__)
#define __INIT_DEP_8(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_7(__VA_ARGS__)
#define __INIT_DEP_9(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_8(__VA_ARGS__)
#define __INIT_DEP_10(d, ...) __INIT_STRINGIFY(d), __INIT_DEP_9(__VA_ARGS__)
#define __INIT_FOREACH_STRINGIFY(...)  __INIT_CONCAT(__INIT_DEP_, __HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define INIT_DEFINE_ROUTINE2(phase, _name, _flags, _function, ...) \
    /* Put the dependencies in a null-terminated string list */ \
    static const char *__initcall_ ## phase ## _ ## _name ## _deps[] = { __INIT_CONCAT(__INIT_DEP_, __HAS_ARGS(__VA_ARGS__))(__VA_ARGS__) }; \
    kernel_initcall_t ___initcall_##phase##_##_name = {\
        .name = #_name, \
        .deps = __initcall_ ## phase ## _ ## _name ## _deps, \
        .function = _function, \
        .flags = _flags, \
    }; \
    __attribute__((section(".initcall." #phase), used)) kernel_initcall_t *__initcall_##phase##_##_name = &___initcall_##phase##_##_name;


#define INIT_DEFINE_ROUTINE(phase, _name, _function, ...) INIT_DEFINE_ROUTINE2(phase, _name, _function, __VA_ARGS__)

#define KERN_EARLY_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_KERN_EARLY, _name, _flags, _function, __VA_ARGS__)
#define FS_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_FS, _name, _flags, _function, __VA_ARGS__)
#define NET_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_NET, _name, _flags, _function, __VA_ARGS__)
#define SCHED_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_SCHED, _name, _flags, _function, __VA_ARGS__)
#define ROOTFS_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_ROOTFS, _name, _flags, _function, __VA_ARGS__)
#define DRIVER_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_DRIVER, _name, _flags, _function, __VA_ARGS__)
#define KERN_LATE_INIT_ROUTINE(_name, _flags, _function, ...) INIT_DEFINE_ROUTINE(PHASE_KERN_LATE, _name, _flags, _function, __VA_ARGS__)

#define INIT_RUN_PHASE(phase) __init_run_phase(__INIT_STRINGIFY(phase))

/**** FUNCTIONS ****/

/**
 * @brief Execute an initcall phase
 */
void __init_run_phase(const char *phase);


#endif