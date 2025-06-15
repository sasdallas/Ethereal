/**
 * @file userspace/lib/graphics/draw.c
 * @brief Graphics drawing
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
#include <graphics/draw.h>
#include <stdlib.h>

/* util macros */
#define GFX_MIN(a, b) (((int)a < (int)b) ? (int)a : (int)b)
#define GFX_MAX(a, b) (((int)a > (int)b) ? (int)a : (int)b)


/**
 * @brief Draw a rectangle to the screen
 * @param ctx The context to draw a rectangle to
 * @param rect The rectangle to draw
 * @param color The color to draw it as
 */
void gfx_drawRectangle(struct gfx_context *ctx, gfx_rect_t *rect, gfx_color_t color) {
	if (!rect || !ctx) return;

	int32_t x = rect->x;
    int32_t y = rect->y;
    int32_t width = rect->width;
    int32_t height = rect->height;

	// Draw the two vertical sides
	for (uint16_t _y = 0; _y < height; _y++) {
		GFX_PIXEL(ctx, x, _y + y) = color; 
		GFX_PIXEL(ctx, x + width, _y + y) = color; 
	}

	// Draw the two horizontal sides
	for (uint16_t _x = 0; _x < width; _x++) {
		GFX_PIXEL(ctx, _x + x, y) = color;
		GFX_PIXEL(ctx, _x + x, y + height) = color;
	}
}

/**
 * @brief Draw a rectangle to the screen
 * @param ctx The context to draw a rectangle to
 * @param rect The rectangle to draw
 * @param color The color to draw it as
 */
void gfx_drawRectangleFilled(struct gfx_context *ctx, gfx_rect_t *rect, gfx_color_t color) {
    if (!rect || !ctx) return;

    int32_t x = rect->x;
    int32_t y = rect->y;
    int32_t width = rect->width;
    int32_t height = rect->height;

    int32_t _left   = GFX_MAX(x, 0);
	int32_t _top    = GFX_MAX(y, 0);
	int32_t _right  = GFX_MIN(x + width,  ctx->width - 1);
	int32_t _bottom = GFX_MIN(y + height, ctx->height - 1);
	for (uint16_t _y = 0; _y < height; ++_y) {
		for (uint16_t _x = 0; _x < width; ++_x) {
			if (x + _x < _left || x + _x > _right || y + _y < _top || y + _y > _bottom)
				continue;
			GFX_PIXEL(ctx, x + _x, y + _y) = color;
		}
	}
}