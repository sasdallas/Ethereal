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
#include <stdatomic.h>

/**** TYPES ****/

/* refcount */
typedef atomic_int refcount_t;

/**** MACROS ****/


#define refcount_inc(ref) ({ atomic_fetch_add(ref, 1); })
#define refcount_dec(ref) ({ atomic_fetch_sub(ref, 1); })
#define refcount_init(ref, val) ({ atomic_store(ref, val); })

#endif