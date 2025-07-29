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
#include <math.h>



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
		if (ctx->flags & CTX_NO_BACKBUFFER) {
			GFX_PIXEL_REAL(ctx, x, _y + y) = color; 
			GFX_PIXEL_REAL(ctx, x + width, _y + y) = color; 
		} else {
			GFX_PIXEL(ctx, x, _y + y) = color; 
			GFX_PIXEL(ctx, x + width, _y + y) = color; 
		}
	}

	// Draw the two horizontal sides
	for (uint16_t _x = 0; _x < width; _x++) {
		if (ctx->flags & CTX_NO_BACKBUFFER) {
			GFX_PIXEL_REAL(ctx, _x + x, y) = color;
			GFX_PIXEL_REAL(ctx, _x + x, y + height) = color;
		} else {
			GFX_PIXEL(ctx, _x + x, y) = color;
			GFX_PIXEL(ctx, _x + x, y + height) = color;
		}
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

			if (ctx->flags & CTX_NO_BACKBUFFER) GFX_PIXEL_REAL(ctx, x + _x, y + _y) = color;
			else GFX_PIXEL(ctx, x + _x, y + _y) = color;
		}
	}
}

/**
 * @brief Draw a rounded rectangle to the screen
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param color The color to draw it as
 * @param radius The radius of the arcs
 */
void gfx_drawRoundedRectangle(struct gfx_context *ctx, gfx_rect_t *rect, gfx_color_t color, int radius) {
	if (!rect || !ctx) return;

	int32_t x = rect->x;
    int32_t y = rect->y;
    int32_t width = rect->width;
    int32_t height = rect->height;

	// Draw the center rectangle
	for (uint16_t _y = 0; _y < height; _y++) {
		for (uint16_t _x = 0; _x < width; _x++) {
			// Check to make sure that we're not going into the arcs we need to do draw
			if ((_x + x > x + width - radius - 1 || (_x + x) < x + radius)) {
				if ((_y + y < y + radius || _y + y > y + height - radius - 1)) {
					continue; // We are going to hit the arcs
				}
			} 

			GFX_PIXEL(ctx, _x + x, _y + y) = gfx_alphaBlend(color, GFX_PIXEL(ctx, _x + x, _y + y));
		}
	}

	// Now do some crazy math to draw each arc.
	float nr = (double)radius + 1.0f;
	for (int dy = 0; dy <= nr; dy++) {
		for (int dx = 0; dx <= nr; dx++) {
			if (nr * nr >= dx * dx + dy * dy) {
				float dist = sqrtf(dx * dx + dy * dy);
				float alpha = 1.0f;
				if (dist > (double)(radius - 1)) {
					alpha = 1.0f - (dist - (double)(radius - 1.0f));
					if (alpha < 0.0f) alpha = 0.0f;
				}

				uint8_t a = (uint8_t)(alpha * 255.0f);

				
				color = GFX_RGBA(GFX_RGB_R(color), GFX_RGB_G(color), GFX_RGB_B(color), ((uint8_t)a));

				if (ctx->flags & CTX_NO_BACKBUFFER) {
					GFX_PIXEL_REAL(ctx, x + radius - dx - 1, y + radius - dy - 1) 				= gfx_alphaBlend(color, GFX_PIXEL_REAL(ctx, x + radius - dx - 1, y + radius - dy - 1));
					GFX_PIXEL_REAL(ctx, x + radius - dx - 1, y + height - radius + dy) 			= gfx_alphaBlend(color, GFX_PIXEL_REAL(ctx, x + radius - dx - 1, y + height - radius + dy));
					GFX_PIXEL_REAL(ctx, x + width - radius + dx, y + radius - dy - 1) 			= gfx_alphaBlend(color, GFX_PIXEL_REAL(ctx, x + width - radius + dx, y + radius - dy - 1));
					GFX_PIXEL_REAL(ctx, x + width - radius + dx, y + height - radius + dy) 		= gfx_alphaBlend(color, GFX_PIXEL_REAL(ctx, x + width - radius + dx, y + height - radius + dy));
				} else {
					GFX_PIXEL(ctx, x + radius - dx - 1, y + radius - dy - 1) 					= gfx_alphaBlend(color, GFX_PIXEL(ctx, x + radius - dx - 1, y + radius - dy - 1));
					GFX_PIXEL(ctx, x + radius - dx - 1, y + height - radius + dy) 				= gfx_alphaBlend(color, GFX_PIXEL(ctx, x + radius - dx - 1, y + height - radius + dy));
					GFX_PIXEL(ctx, x + width - radius + dx, y + radius - dy - 1) 				= gfx_alphaBlend(color, GFX_PIXEL(ctx, x + width - radius + dx, y + radius - dy - 1));
					GFX_PIXEL(ctx, x + width - radius + dx, y + height - radius + dy) 			= gfx_alphaBlend(color, GFX_PIXEL(ctx, x + width - radius + dx, y + height - radius + dy));
				}
			}
		}
	}
}