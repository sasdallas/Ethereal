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
 * @brief Draw a rectangle to the screen with a specific pattern
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param pattern The pattern function
 * @param data Data for the pattern function
 */
void gfx_drawRectangleFilledPattern(struct gfx_context *ctx, gfx_rect_t *rect, gfx_pattern_func_t pattern, void *data) {
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

			gfx_color_t *pixel = (ctx->flags & CTX_NO_BACKBUFFER) ? &GFX_PIXEL_REAL(ctx, _x + x, _y + y) : &GFX_PIXEL(ctx, _x + x, _y + y);
			*pixel = gfx_alphaBlend(pattern(_x, _y, 255, data), *pixel);
		}
	}
}

/**
 * @brief Draw rectangle with gradient
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param type Type of gradient
 * @param start The start of the gradient
 * @param end The end of the gradient
 */
void gfx_drawRectangleFilledGradient(struct gfx_context *ctx, gfx_rect_t *rect, uint8_t type, gfx_color_t start, gfx_color_t end) {
	gfx_gradient_data_t grad = {
		.type = type,
		.start = start,
		.end = end,
		.size = (type == GFX_GRADIENT_VERTICAL) ? rect->height : rect->width,
	};

	return gfx_drawRectangleFilledPattern(ctx, rect, gfx_patternGradient, &grad);
}


/**
 * @brief Draw a rounded rectangle to the screen
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param color The color to draw it as
 */
void gfx_drawRoundedRectangle(struct gfx_context *ctx, gfx_rect_t *rect, int radius, gfx_color_t color) {
    if (!rect || !ctx) return;

    int32_t x = rect->x;
    int32_t y = rect->y;
    int32_t width  = rect->width;
    int32_t height = rect->height;

    int32_t max_w = GFX_WIDTH(ctx);
    int32_t max_h = GFX_HEIGHT(ctx);

    // Clip to the screen bounds
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + width  > max_w ? max_w : x + width;
    int32_t y1 = y + height > max_h ? max_h : y + height;

    if (x0 >= x1 || y0 >= y1) return;

	// Center rectangle
    for (int32_t py = y0; py < y1; py++) {
        for (int32_t px = x0; px < x1; px++) {

            int32_t lx = px - x;
            int32_t ly = py - y;

            if ((lx < radius || lx >= width - radius) &&
                (ly < radius || ly >= height - radius)) {
                continue;
            }

            GFX_PIXEL(ctx, px, py) =
                gfx_alphaBlend(color, GFX_PIXEL(ctx, px, py));
        }
    }

	// Corners
    float nr = (float)radius + 1.0f;

    for (int dy = 0; dy <= nr; dy++) {
        for (int dx = 0; dx <= nr; dx++) {

            if (nr * nr < dx * dx + dy * dy) continue;

            float dist = sqrtf(dx * dx + dy * dy);
            float alpha = 1.0f;

            if (dist > (float)(radius - 1)) {
                alpha = 1.0f - (dist - (float)(radius - 1));
                if (alpha < 0.0f) alpha = 0.0f;
            }

            uint8_t a = (uint8_t)(alpha * 255.0f);

            gfx_color_t c = GFX_RGBA(
                GFX_RGB_R(color) * a / 255,
                GFX_RGB_G(color) * a / 255,
                GFX_RGB_B(color) * a / 255,
                a
            );

            int16_t px[4] = {
                x + radius - dx - 1,
                x + width  - radius + dx,
                x + radius - dx - 1,
                x + width  - radius + dx
            };

            int16_t py[4] = {
                y + radius - dy - 1,
                y + radius - dy - 1,
                y + height - radius + dy,
                y + height - radius + dy
            };

            for (int i = 0; i < 4; i++) {
                if (px[i] < 0 || px[i] >= max_w ||
                    py[i] < 0 || py[i] >= max_h)
                    continue;

                if (ctx->flags & CTX_NO_BACKBUFFER) {
                    GFX_PIXEL_REAL(ctx, px[i], py[i]) =
                        gfx_alphaBlend(c, GFX_PIXEL_REAL(ctx, px[i], py[i]));
                } else {
                    GFX_PIXEL(ctx, px[i], py[i]) =
                        gfx_alphaBlend(c, GFX_PIXEL(ctx, px[i], py[i]));
                }
            }
        }
    }
}

/**
 * @brief Draw a rounded rectangle to the screen with a pattern
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param pattern The pattern function to use
 * @param data Additional data to the pattern function
 */
void gfx_drawRoundedRectanglePattern(struct gfx_context *ctx, gfx_rect_t *rect, int radius, gfx_pattern_func_t pattern, void *data) {
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

			gfx_color_t c = pattern(_x, _y, 255, data);
			GFX_PIXEL(ctx, _x + x, _y + y) = gfx_alphaBlend(c, GFX_PIXEL(ctx, _x + x, _y + y));
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
				
				// Get the coordinates
				int16_t _x = x + radius - dx - 1;
				int16_t _x2 = x + width - radius + dx;
				int16_t _y = y + radius - dy - 1;
				int16_t _y2 = y + height - radius + dy;

		

				// Plot them
				if (ctx->flags & CTX_NO_BACKBUFFER) {
					GFX_PIXEL_REAL(ctx, _x, _y) 		= gfx_alphaBlend(pattern(_x - x, _y - y, a, data), GFX_PIXEL_REAL(ctx, _x, _y));
					GFX_PIXEL_REAL(ctx, _x, _y2) 		= gfx_alphaBlend(pattern(_x - x, _y2 - y, a, data), GFX_PIXEL_REAL(ctx, _x, _y2));
					GFX_PIXEL_REAL(ctx, _x2, _y) 		= gfx_alphaBlend(pattern(_x2 - x, _y - y, a, data), GFX_PIXEL_REAL(ctx, _x2, _y));
					GFX_PIXEL_REAL(ctx, _x2, _y2) 		= gfx_alphaBlend(pattern(_x2 - x, _y2 - y, a, data), GFX_PIXEL_REAL(ctx, _x2, _y2));
				} else {
					GFX_PIXEL(ctx, _x, _y) 			= gfx_alphaBlend(pattern(_x - x, _y - y, a, data), GFX_PIXEL(ctx, _x, _y));
					GFX_PIXEL(ctx, _x, _y2) 		= gfx_alphaBlend(pattern(_x - x, _y2 - y, a, data), GFX_PIXEL(ctx, _x, _y2));
					GFX_PIXEL(ctx, _x2, _y) 		= gfx_alphaBlend(pattern(_x2 - x, _y - y, a, data), GFX_PIXEL(ctx, _x2, _y));
					GFX_PIXEL(ctx, _x2, _y2) 		= gfx_alphaBlend(pattern(_x2 - x, _y2 - y, a, data), GFX_PIXEL(ctx, _x2, _y2));
				}
			}
		}
	}
}

/**
 * @brief Draw rounded rectangle with gradient
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param type Type of gradient
 * @param start Starting color of gradient
 * @param end Ending color of gradient
 */
void gfx_drawRoundedRectangleGradient(struct gfx_context *ctx, gfx_rect_t *rect, int radius, uint8_t type, gfx_color_t start, gfx_color_t end) {
	gfx_gradient_data_t grad = {
		.type = type,
		.start = start,
		.end = end,
		.size = (type == GFX_GRADIENT_VERTICAL) ? rect->height : rect->width,
	};

	return gfx_drawRoundedRectanglePattern(ctx, rect, radius, gfx_patternGradient, &grad);
}