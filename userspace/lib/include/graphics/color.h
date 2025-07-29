/**
 * @file userspace/lib/include/graphics/color.h
 * @brief Graphics color
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_COLOR_H
#define _GRAPHICS_COLOR_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef uint32_t gfx_color_t;

/**** MACROS ****/

/* Color manipulation */
#define GFX_RGB_A(color) (((color) & 0xFF000000) / 0x1000000)
#define GFX_RGB_R(color) (((color) & 0x00FF0000) / 0x10000)
#define GFX_RGB_G(color) (((color) & 0x0000FF00) / 0x100)
#define GFX_RGB_B(color) (((color) & 0x000000FF))
#define GFX_RGBA(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define GFX_RGB(r, g, b) GFX_RGBA(r, g, b, 255)


#endif