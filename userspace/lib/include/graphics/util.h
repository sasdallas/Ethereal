/**
 * @file userspace/lib/include/graphics/util.h
 * @brief Utility graphical functions
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_UTIL_H
#define _GRAPHICS_UTIL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/gfx.h>

/**** INLINE FUNCTIONS ****/

static inline int gfx_clamp(int val, int low, int high) {
    if (val < low) return low;
    if (val > high) return high;
    return val; 
}


#endif