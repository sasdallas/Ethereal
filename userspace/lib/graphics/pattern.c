/**
 * @file userspace/lib/graphics/pattern.c
 * @brief Some patterns to use (gradients, default, etc)
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

FILE *f = NULL;

/**
 * @brief Default pattern function (for functions that wanna thunk to this)
 * @param data Pointer to graphics color object
 */
gfx_color_t gfx_patternDefault(int32_t x, int32_t y, uint8_t alpha, void *data) {
    gfx_color_t c = *(gfx_color_t*)data;
    return GFX_RGBA(GFX_RGB_R(c), GFX_RGB_G(c), GFX_RGB_B(c), GFX_RGB_A(c) * alpha);
}

/**
 * @brief Gradient drawer
 * @param data @c gfx_gradient_data_t object
 */
gfx_color_t gfx_patternGradient(int32_t x, int32_t y, uint8_t alpha, void *data) {
    gfx_gradient_data_t *grad = (gfx_gradient_data_t*)data;

    // Calculate the step
    int32_t use = (grad->type == GFX_GRADIENT_VERTICAL) ? y : x;
    float point = (use) / (double)grad->size;

    // We have a point in the gradient, now let's make some colors
    uint8_t a = (GFX_RGB_A(grad->start) * (1.0 - point)) + (GFX_RGB_A(grad->end) * (point));
    uint8_t r = (GFX_RGB_R(grad->start) * (1.0 - point)) + (GFX_RGB_R(grad->end) * (point));
    uint8_t g = (GFX_RGB_G(grad->start) * (1.0 - point)) + (GFX_RGB_G(grad->end) * (point));
    uint8_t b = (GFX_RGB_B(grad->start) * (1.0 - point)) + (GFX_RGB_B(grad->end) * (point));
    
    gfx_color_t c =  0 |
            ((a * alpha / 255) & 0xFF) << 24 |
            ((r * alpha / 255) & 0xFF) << 16 |
            ((g * alpha / 255) & 0xFF) << 8 |
            ((b * alpha / 255) & 0xFF) << 0;

    return c;
}