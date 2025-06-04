/**
 * @file userspace/lib/graphics/blend.c
 * @brief Alpha blend support
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <graphics/gfx.h>

/**
 * @brief Alpha blend two colors together
 * @param top The topmost color
 * @param bottom The bottom color
 */
gfx_color_t gfx_alphaBlend(gfx_color_t top, gfx_color_t bottom) {
    // Quick alpha cases
    if (GFX_RGB_A(top) == 0) return bottom; // Top is fully transparent
    if (GFX_RGB_A(bottom) == 0) return top;
    if (GFX_RGB_A(top) == 255) return top;

    // Source: https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/lib/graphics.c#L282
    // (temporary steal)
    uint8_t a = GFX_RGB_A(top);
    uint16_t t = 0xFF ^ a;
    uint8_t d_r = GFX_RGB_R(top) + (((uint32_t)(GFX_RGB_R(bottom) * t + 128) * 257) >> 16UL);
    uint8_t d_g = GFX_RGB_G(top) + (((uint32_t)(GFX_RGB_G(bottom) * t + 128) * 257) >> 16UL);
    uint8_t d_b = GFX_RGB_B(top) + (((uint32_t)(GFX_RGB_B(bottom) * t + 128) * 257) >> 16UL);
    uint8_t d_a = GFX_RGB_A(top) + (((uint32_t)(GFX_RGB_A(bottom) * t + 128) * 257) >> 16UL);
    return GFX_RGBA(d_r, d_g, d_b, d_a);
}