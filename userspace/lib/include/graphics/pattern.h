/**
 * @file userspace/lib/include/graphics/pattern.h
 * @brief Patterns
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_PATTERN_H
#define _GRAPHICS_PATTERN_H

/**** INCLUDES ****/
#include <graphics/gfx.h>
#include <graphics/color.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define GFX_GRADIENT_VERTICAL           0
#define GFX_GRADIENT_HORIZONTAL         1

/**** TYPES ****/

/**
 * @brief Graphics pattern function
 * @param x The current X position (relative)
 * @param y The current Y position (relative)
 * @param alpha The current alpha vector in use (AA)
 * @param data Additional data
 */
typedef gfx_color_t (*gfx_pattern_func_t)(int32_t x, int32_t y, uint8_t alpha, void *data);

typedef struct gfx_gradient_data {
    uint8_t type;                       // Type
    size_t size;                        // EITHER HEIGHT OR WIDTH
    gfx_color_t start;                  // Start
    gfx_color_t end;                    // End
} gfx_gradient_data_t;

/**** FUNCTIONS ****/

/**
 * @brief Default pattern function (for functions that wanna thunk to this)
 * @param data Graphics color object
 */
gfx_color_t gfx_patternDefault(int32_t x, int32_t y, uint8_t alpha, void *data);

/**
 * @brief Gradient drawer
 * @param data @c gfx_gradient_data_t object
 */
gfx_color_t gfx_patternGradient(int32_t x, int32_t y, uint8_t alpha, void *data);

#endif 