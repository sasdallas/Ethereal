/**
 * @file userspace/lib/graphics/blend.c
 * @brief Alpha blend support
 * 
 * 
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2011-2021 K. Lange
 */

#include <graphics/gfx.h>

/**
 * @brief Alpha blend two colors together
 * @param top The topmost color
 * @param bottom The bottom color
 */
inline gfx_color_t gfx_alphaBlend(gfx_color_t top, gfx_color_t bottom) {
    // TODO: Rewrite without stolen code...
	if (GFX_RGB_A(bottom) == 0) return top;
	if (GFX_RGB_A(top) == 255) return top;
	if (GFX_RGB_A(top) == 0) return bottom;
	uint8_t a = GFX_RGB_A(top);
	uint16_t t = 0xFF ^ a;
	uint8_t d_r = GFX_RGB_R(top) + (((uint32_t)(GFX_RGB_R(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_g = GFX_RGB_G(top) + (((uint32_t)(GFX_RGB_G(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_b = GFX_RGB_B(top) + (((uint32_t)(GFX_RGB_B(bottom) * t + 0x80) * 0x101) >> 16UL);
	uint8_t d_a = GFX_RGB_A(top) + (((uint32_t)(GFX_RGB_A(bottom) * t + 0x80) * 0x101) >> 16UL);
	return GFX_RGBA(d_r, d_g, d_b, d_a);
}