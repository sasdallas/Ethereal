/**
 * @file hexahedron/subsystems/timemonitor.c
 * @brief Timer monitoring system
 * 
 * Monitors the time spent in each CPU in order to produce usage for each process.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/subsystems/timemonitor.h>
#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>

/**
 * @brief Get nanoseconds
 */
uint64_t timemonitor_getNanoseconds() {
    // when clockv2 is finished, use that
    unsigned long sec, subsec;
    clock_getCurrentTime(&sec, &subsec);
    return sec * 1000000000 + subsec * 1000;
}

/**
 * @brief Setup time for process
 */
void timemonitor_updateProcessStart(thread_t *thr) {
    thr->times.last_update_time = timemonitor_getNanoseconds();
    thr->times.stime = 0;
    thr->times.utime = 0;
    thr->times.in_system_call = false;
}

/**
 * @brief Update process times on scheduler context switch 
 */
void timemonitor_updateContextSwitch() {
    thread_t *thr = current_cpu->current_thread;
    uint64_t ns = timemonitor_getNanoseconds();
    uint64_t delta = ns - thr->times.last_update_time;

    if (thr->times.in_system_call) {
        thr->times.stime += delta;
        current_cpu->times.times[CPU_TIME_SYS] += delta;
    } else {
        thr->times.utime += delta;
        current_cpu->times.times[CPU_TIME_USER] += delta;
    }

    thr->times.last_update_time = ns;
}

/**
 * @brief Update process times on thread exit
 */
void timemonitor_updateThreadExit() {
    // dead threads are left in the process list, which is fine, but we need to update
    // their times since they call process_switchNextThread()
    thread_t *thr = current_cpu->current_thread;
    uint64_t ns = timemonitor_getNanoseconds();
    uint64_t delta = ns - thr->times.last_update_time;
    thr->times.stime += delta;
    current_cpu->times.times[CPU_TIME_SYS] += delta;
}

/**
 * @brief Update process times on idle task entry
 */
void timemonitor_updateIdleEntry() {
    current_cpu->times.last_measurement = timemonitor_getNanoseconds(); 
}

/**
 * @brief Update process times on idle task exit
 */
void timemonitor_updateIdleExit() {
    current_cpu->times.times[CPU_TIME_IDLE] += timemonitor_getNanoseconds() - current_cpu->times.last_measurement;
}

/**
 * @brief Update process times on syscall entry
 */
void timemonitor_updateSyscallEntry() {
    uint64_t ns = timemonitor_getNanoseconds();
    thread_t *thr = current_cpu->current_thread;
    uint64_t delta = ns - thr->times.last_update_time;

    thr->times.utime += delta;
    current_cpu->times.times[CPU_TIME_USER] += delta;

    thr->times.in_system_call = true;
    thr->times.last_update_time = ns;
}

/**
 * @brief Update process times on syscall exit
 */
void timemonitor_updateSyscallExit() {
    uint64_t ns = timemonitor_getNanoseconds();
    thread_t *thr = current_cpu->current_thread;
    uint64_t delta = ns - thr->times.last_update_time;

    thr->times.stime += delta;
    current_cpu->times.times[CPU_TIME_SYS] += delta;

    thr->times.in_system_call = false;
    thr->times.last_update_time = ns;
}

/**
 * @brief Update process time on sleep enter
 * 
 * Sleep time isn't accounted for in stime/utime
 */
void timemonitor_updateSleepEnter() {
    uint64_t ns = timemonitor_getNanoseconds();
    thread_t *thr = current_cpu->current_thread;
    uint64_t delta = ns - thr->times.last_update_time;

    thr->times.stime += delta;
    current_cpu->times.times[CPU_TIME_SYS] += delta;

    thr->times.in_system_call = false;
    thr->times.last_update_time = ns;
}

/**
 * @brief Update process time on sleep exit
 * 
 * Sleep time isn't accounted for in stime/utime
 */
void timemonitor_updateSleepExit() {
    current_cpu->current_thread->times.last_update_time = timemonitor_getNanoseconds();
}

/**
 * @brief Update process times on IRQ entry
 */
void timemonitor_updateIrqEntry() {
    if (IN_INTERRUPT()) return; // this is called before IRQ_ENTER so

    uint64_t ns = timemonitor_getNanoseconds();
    if (current_cpu->current_thread) {
        thread_t *thr = current_cpu->current_thread;

        if (current_cpu->current_process == current_cpu->idle_process) {
            // Idle process is running, add the idle time spent so far
            current_cpu->times.times[CPU_TIME_IDLE] += ns - current_cpu->times.last_measurement;
        }

        if (thr->times.in_system_call) {
            thr->times.stime += ns - thr->times.last_update_time;
        } else {
            thr->times.utime += ns - thr->times.last_update_time;
        }
    }
    
    current_cpu->times.last_measurement = ns;
}

/**
 * @brief Update process times on IRQ exit
 * Only call this on the nested IRQ count being 0
 */
void timemonitor_updateIrqExit() {
    if (IN_INTERRUPT()) return; // more nested

    uint64_t ns = timemonitor_getNanoseconds();
    if (current_cpu->current_thread) {
        thread_t *thr = current_cpu->current_thread;
        thr->times.last_update_time = ns;
    }

    current_cpu->times.times[CPU_TIME_IRQ] += ns - current_cpu->times.last_measurement;
    current_cpu->times.last_measurement = ns;
}
