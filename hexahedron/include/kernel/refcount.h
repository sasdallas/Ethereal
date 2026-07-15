/**
 * @file hexahedron/include/kernel/refcount.h
 * @brief Refcount
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_REFCOUNT_H
#define KERNEL_REFCOUNT_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

/* refcount */
typedef int refcount_t;

/**** MACROS ****/


#define refcount_inc(ref) ({ __atomic_add_fetch(ref, 1, __ATOMIC_SEQ_CST); })
#define refcount_dec(ref) ({ __atomic_sub_fetch(ref, 1, __ATOMIC_SEQ_CST); })
#define refcount_init(ref, val) ({ __atomic_store_n(ref, val, __ATOMIC_SEQ_CST); })

#endif