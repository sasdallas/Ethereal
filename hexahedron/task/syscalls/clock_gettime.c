/**
 * @file hexahedron/task/syscalls/clock_gettime.c
 * @brief Clock gettime
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>

long sys_clock_gettime(int clock, time_t *secs, long *nanos) {
    switch (clock) {
        case CLOCK_REALTIME:
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_RAW: {
            struct timeval tv;
            clock_gettimeofday(&tv, NULL);    
            *secs = tv.tv_sec;
            *nanos = tv.tv_usec * 1000;
            break;
        };

        case CLOCK_BOOTTIME:
            uint64_t s = clock_getBoottime();
            *secs = s;
            *nanos = 0;
            break;

        default:
            SYSCALL_LOG(ERR, "Unimplemented clock ID %d\n", clock);
            return -ENOSYS;
    }
    
    return 0;
}