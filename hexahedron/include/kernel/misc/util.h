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
#include <kernel/debug.h>
#include <ctype.h>
#include <assert.h>

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


#define HEXDUMP(ptr, size)                                                      \
    do {                                                                         \
        const unsigned char *data = (const unsigned char *)(ptr);               \
        size_t length = (size_t)(size);                                         \
        for (size_t i = 0; i < length; i += 16) {                                \
            char hex[16 * 3 + 1] = {0};                                          \
            char ascii[17] = {0};                                                \
            for (size_t j = 0; j < 16 && i + j < length; ++j) {                  \
                unsigned char byte = data[i + j];                                \
                sprintf(&hex[j * 3], "%02X ", byte);                             \
                ascii[j] = isprint(byte) ? byte : '.';                           \
            }                                                                    \
            dprintf(DEBUG, "%08zx  %-48s  |%s\n", i, hex, ascii);                     \
        }                                                                        \
    } while (0)

/* min/max */
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/* Aligning macros */
#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align)-1))
#define ALIGN_DOWN(val, align) (((val) & ~((align)-1)))
#define IS_ALIGNED(val, align) (((val) & ((align)-1))==0)

/* Critical */
#define __NON_INTERRUPTABLE_BEGIN()  int __state = hal_getInterruptState(); hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
#define __NON_INTERRUPTABLE_END() hal_setInterruptState(__state); 

#define __PREEMPT_DISABLE() int __flags=0; if (current_cpu->current_thread) { __flags = current_cpu->current_thread->flags; __sync_or_and_fetch(&current_cpu->current_thread->flags, THREAD_FLAG_NO_PREEMPT); }
#define __PREEMPT_ENABLE() if (current_cpu->current_thread) current_cpu->current_thread->flags = __flags;

/* refcount */
typedef atomic_int refcount_t;
#define refcount_inc(ref) ({ atomic_fetch_add(ref, 1); })
#define refcount_dec(ref) ({ atomic_fetch_sub(ref, 1); })
#define refcount_init(ref, val) ({ atomic_store(ref, val); })

/* stub */
#define STUB() kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "stub", "%s:%d: \"%s\" is a stub", __FILE__, __LINE__, __func__); __builtin_unreachable();

/* caller */
#define CALLER __builtin_return_address(0)

#endif