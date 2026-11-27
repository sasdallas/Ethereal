/**
 * @file hexahedron/include/kernel/gfx/gfx.h
 * @brief Minimal graphics routines
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_GFX_GFX_H
#define KERNEL_GFX_GFX_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/gfx/color.h>

/**** FUNCTIONS ****/

/**
 * @brief Draw the Hexahedron logo
 * @param color The logo color
 */
void gfx_drawLogo(color_t color);

#endif
