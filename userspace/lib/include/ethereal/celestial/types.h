/**
 * @file userspace/lib/include/ethereal/celestial/types.h
 * @brief Celestial types (win_pos_t)
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_TYPES_H
#define _CELESTIAL_TYPES_H

/**** INCLUDES ****/
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <ethereal/shared.h>

/**** TYPES ****/

typedef size_t win_pos_t;
typedef pid_t wid_t;

#endif