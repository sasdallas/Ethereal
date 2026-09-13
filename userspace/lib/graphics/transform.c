/**
 * @file userspace/lib/graphics/transform.c
 * @brief Exists only to contain the sprite transformation drawing
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
#include <math.h>

// lerp function for bilinear
static gfx_color_t lerp(gfx_color_t a, gfx_color_t b, float _t) {
    uint16_t t = (uint16_t)(_t * 256.0f + 0.5f);
    uint16_t pl = 0x100 - t; // weight for left color

    uint8_t r = (((uint32_t)GFX_RGB_R(a) * pl + (uint32_t)GFX_RGB_R(b) * t + 0x80) >> 8);
    uint8_t g = (((uint32_t)GFX_RGB_G(a) * pl + (uint32_t)GFX_RGB_G(b) * t + 0x80) >> 8);
    uint8_t b_ = (((uint32_t)GFX_RGB_B(a) * pl + (uint32_t)GFX_RGB_B(b) * t + 0x80) >> 8);
    uint8_t a_ = (((uint32_t)GFX_RGB_A(a) * pl + (uint32_t)GFX_RGB_A(b) * t + 0x80) >> 8);

    return GFX_RGBA(r, g, b_, a_);
}

/**
 * @brief Draw a sprite using a transformation matrix
 * @param ctx The context to draw to
 * @param sprite The sprite to draw
 * @param matrix The matrix to use to draw the sprite
 * @param alpha The alpha vector with which to use the sprite drawing
 */
void gfx_renderSpriteTransform(gfx_context_t *ctx, sprite_t *sprite, gfx_mat2x3_t *matrix, uint8_t alpha) {
    // First apply point transformations
    float x0, y0; // Upper left corner
    float x1, y1; // Upper right corner
    float x2, y2; // Lower right corner
    float x3, y3; // Lower left corner

    gfx_mat2x3_transform(matrix, 0.0, 0.0, &x0, &y0);
    gfx_mat2x3_transform(matrix, (float)sprite->width, 0.0, &x1, &y1);
    gfx_mat2x3_transform(matrix, (float)sprite->width, (float)sprite->height, &x2, &y2);
    gfx_mat2x3_transform(matrix, 0.0, (float)sprite->height, &x3, &y3);

    // Of course, clamp to bounds
    int32_t x_low = gfx_clamp(fminf(fminf(x0,x1), fminf(x2, x3)), 0, GFX_WIDTH(ctx));
    int32_t x_max = gfx_clamp(fmaxf(fmaxf(x0,x1), fmaxf(x2, x3)), 0, GFX_WIDTH(ctx));
    int32_t y_low = gfx_clamp(fminf(fminf(y0,y1), fminf(y2, y3)), 0, GFX_HEIGHT(ctx));
    int32_t y_max = gfx_clamp(fmaxf(fmaxf(y0,y1), fmaxf(y2, y3)), 0, GFX_HEIGHT(ctx));

    // Invert the matrix to get the transformation X and Y
    gfx_mat2x3_t invert;
    gfx_mat2x3_invert(matrix, &invert);
    float tx = -(invert.m[0] * matrix->m[2] + invert.m[1] * matrix->m[5]);
    float ty = -(invert.m[3] * matrix->m[2] + invert.m[4] * matrix->m[5]);

    sprite_t *line = gfx_createSprite(x_max - x_low, 1);

    for (int y = y_low; y < y_max; y++) {
        // Convert the screen pixel back to a sprite coordinate pair
        float u = invert.m[0] * (float)x_low + invert.m[1] * (float)y + tx;
        float v = invert.m[3] * (float)x_low + invert.m[4] * (float)y + ty;
    
        
        for (int32_t x = x_low; x < x_max; x++) {
            // Bilinearly interpolate at X coordinate u and Y coordinate v
            // Clamp pixels down to get rid of anything silly
            float sample_x = fmaxf(fminf(u, sprite->width - 1), 0);
            float sample_y = fmaxf(fminf(v, sprite->height - 1), 0);
            int _x = (int)floorf(sample_x);
            int _y = (int)floorf(sample_y);
            int _x2 = GFX_MIN(_x + 1, (int)sprite->width - 1);
            int _y2 = GFX_MIN(_y + 1, (int)sprite->height - 1);

            gfx_color_t c00 = SPRITE_PIXEL(sprite, _x, _y);
            gfx_color_t c01 = SPRITE_PIXEL(sprite, _x2, _y);
            gfx_color_t c10 = SPRITE_PIXEL(sprite, _x, _y2);
            gfx_color_t c11 = SPRITE_PIXEL(sprite, _x2, _y2);

            // FINISH
            float dx = sample_x - (float)_x;
            float dy = sample_y - (float)_y;

            gfx_color_t top = lerp(c00, c01, dx);
            gfx_color_t bot = lerp(c10, c11, dx);

            SPRITE_PIXEL(line, x - x_low, 0) = lerp(top, bot, dy);

            u += invert.m[0];
            v += invert.m[3];
        }

        gfx_renderSpriteAlpha(ctx, line, x_low, y, alpha);
    }

    gfx_destroySprite(line);

}
