/**
 * @file hexahedron/include/kernel/misc/util.h
 * @brief Utility macros
 * 
 * Do note that some of these won't be in use across the whole codebase.
 * This is a recent addition
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_UTIL_H
#define KERNEL_MISC_UTIL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <time.h>
#include <kernel/drivers/clock.h>

/**** MACROS ****/

/* Range macros */
#define IN_RANGE(a, b, c)               (((a) >= (b)) && ((a) <= (c)))              // Returns whether a is in range b-c (inclusive)
#define IN_RANGE_EXCLUSIVE(a, b, c)     (IN_RANGE(a, b+1, c-1))                     // Returns whether a is in range b-c (exclusive)
#define RANGE_IN_RANGE(a1, b1, a2, b2)  (((a1) >= (a2) && (((b1)) <= b2)))          // Returns whether range a1-b1 is contained in range a2-b2 (inclusive)
#define RANGE_IN_RANGE_EXCLUSIVE(a1,b1,a2,b2) (RANGE_IN_RANGE(a1, b1, a2+1, b2-1))  // Returns whether range a1-b1 is contained in range a2-b2 (exclusive)

/* Timing macros */
#define PROFILE_START() struct timeval t; \
                        gettimeofday(&t, NULL);

#define PROFILE_END()   { struct timeval t_now; \
                        gettimeofday(&t_now, NULL); \
                        dprintf(DEBUG, "%s: Profiling complete. Elapsed: %ds %dusec\n", __FUNCTION__, t_now.tv_sec - t.tv_sec, t_now.tv_usec - t.tv_usec); }

/* Timeout macro (0 on success, 1 on timeout) */
#define TIMEOUT(cond, ms) ({ int64_t timeout = (ms); while (!(cond)) { clock_sleep(25); timeout -= 25; if (timeout <= 0) break; }; !(timeout > 0); })

/* Static asserts - https://stackoverflow.com/questions/3385515/static-assert-in-c */
/* !!!: Yes this is hacky, but it's at least not GCC specific */

#define STATIC_ASSERT_MSG(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#define __STATIC_ASSERT3(cond, file, line) STATIC_ASSERT_MSG((cond), at_line_##line)
#define __STATIC_ASSERT2(cond, file, line) __STATIC_ASSERT3(cond, file, line)
#define STATIC_ASSERT(cond) __STATIC_ASSERT2(cond, __FILE__, __LINE__)

/* min/max */
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#endif