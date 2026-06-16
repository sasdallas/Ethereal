/**
 * @file hexahedron/include/kernel/subsystems/timemonitor.h
 * @brief Time tracking subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_TIME_TRACKER_H
#define KERNEL_SUBSYSTEMS_TIME_TRACKER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>

/**** DEFINITIONS ****/

#define CPU_TIME_USER 0
#define CPU_TIME_IDLE 1
#define CPU_TIME_SYS 2
#define CPU_TIME_IRQ 3

/**** TYPES ****/

struct process;
struct thread;

typedef struct _cpu_times {
    uint64_t times[4];
    uint64_t last_measurement;
} cpu_times_t;

typedef struct _thread_times {
    uint64_t utime;
    uint64_t stime;
    uint64_t last_update_time;

    // !!! serious hack as the kernel doesn't have a good way to check whether a task was in kernel mode.
    // TODO: add a way for the kernel to tell whether a task was in user mode or kernel mode at time of preemption
    bool in_system_call; 
} thread_times_t;

/**** FUNCTIONS ****/

/**
 * @brief Setup time for process
 */
void timemonitor_updateProcessStart(struct thread *thr);

/**
 * @brief Update process times on scheduler context switch 
 */
void timemonitor_updateContextSwitch();

/**
 * @brief Update process times on thread exit
 */
void timemonitor_updateThreadExit();

/**
 * @brief Update process times on idle task entry
 */
void timemonitor_updateIdleEntry();

/**
 * @brief Update process times on idle task exit
 */
void timemonitor_updateIdleExit();

/**
 * @brief Update process times on syscall entry
 */
void timemonitor_updateSyscallEntry();

/**
 * @brief Update process times on syscall exit
 */
void timemonitor_updateSyscallExit();

/**
 * @brief Update process times on IRQ entry
 */
void timemonitor_updateIrqEntry();

/**
 * @brief Update process times on IRQ exit
 * Only call this on the nested IRQ count being 0
 */
void timemonitor_updateIrqExit();

/**
 * @brief Update process time on sleep enter
 * 
 * Sleep time isn't accounted for in stime/utime
 */
void timemonitor_updateSleepEnter();

/**
 * @brief Update process time on sleep exit
 * 
 * Sleep time isn't accounted for in stime/utime
 */
void timemonitor_updateSleepExit();

#endif
