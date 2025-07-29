/**
 * @file userspace/lib/include/graphics/draw.h
 * @brief Drawing functions for graphics library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_DRAW_H
#define _GRAPHICS_DRAW_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/pattern.h>
#include <graphics/color.h>

/**** TYPES ****/

/**
 * @brief Rectangle structure
 */
typedef struct gfx_rect {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} gfx_rect_t;

/**** MACROS ****/

#define GFX_RECT(rx, ry, rw, rh) ((gfx_rect_t){ .x = rx, .y = ry, .width = rw, .height = rh})

// Yet another max/min declaration
#define GFX_RECT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define GFX_RECT_MIN(a, b) ((a) < (b) ? (a) : (b))

#define GFX_RECT_LEFT(ctx, r) (GFX_RECT_MAX(r.x, 0))
#define GFX_RECT_RIGHT(ctx, r) (GFX_RECT_MIN(r.x + r.width, GFX_WIDTH(ctx) - 1))
#define GFX_RECT_TOP(ctx, r) (GFX_RECT_MAX(r.y, 0))
#define GFX_RECT_BOTTOM(ctx, r) (GFX_RECT_MIN(r.y + r.height, GFX_HEIGHT(ctx) - 1))

#define GFX_RECT_COLLIDES(ctx, r1, r2) ((GFX_RECT_LEFT(ctx, r1) < GFX_RECT_RIGHT(ctx, r2)) && (GFX_RECT_LEFT(ctx, r2) < GFX_RECT_RIGHT(ctx, r1)) && \
                                            (GFX_RECT_TOP(ctx, r1) < GFX_RECT_BOTTOM(ctx, r2)) && (GFX_RECT_TOP(ctx, r2) < GFX_RECT_BOTTOM(ctx, r1)))

/**** FUNCTIONS ****/

struct gfx_context;

/**
 * @brief Draw a rectangle to the screen
 * @param ctx The context to draw a rectangle to
 * @param rect The rectangle to draw
 * @param color The color to draw it as
 */
void gfx_drawRectangle(struct gfx_context *ctx, gfx_rect_t *rect, gfx_color_t color);

/**
 * @brief Draw a rectangle to the screen and fill it
 * @param ctx The context to draw a rectangle to
 * @param rect The rectangle to draw
 * @param collor The color to draw it as
 */
void gfx_drawRectangleFilled(struct gfx_context *ctx, gfx_rect_t *rect, gfx_color_t color);

/**
 * @brief Draw a rectangle to the screen with a specific pattern
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param pattern The pattern function
 * @param data Data for the pattern function
 */
void gfx_drawRectangleFilledPattern(struct gfx_context *ctx, gfx_rect_t *rect, gfx_pattern_func_t pattern, void *data);

/**
 * @brief Draw rectangle with gradient
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param type Type of gradient
 * @param start The start of the gradient
 * @param end The end of the gradient
 */
void gfx_drawRectangleFilledGradient(struct gfx_context *ctx, gfx_rect_t *rect, uint8_t type, gfx_color_t start, gfx_color_t end);

/**
 * @brief Draw a rounded rectangle to the screen
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param color The color to draw it as
 */
void gfx_drawRoundedRectangle(struct gfx_context *ctx, gfx_rect_t *rect, int radius, gfx_color_t color);

/**
 * @brief Draw a rounded rectangle to the screen with a pattern
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param pattern The pattern function to use
 * @param data Additional data to the pattern function
 */
void gfx_drawRoundedRectanglePattern(struct gfx_context *ctx, gfx_rect_t *rect, int radius, gfx_pattern_func_t pattern, void *data);

/**
 * @brief Draw rounded rectangle with gradient
 * @param ctx The context to draw the rectangle in
 * @param rect The rectangle to draw
 * @param radius The radius of the arcs
 * @param type Type of gradient
 * @param start Starting color of gradient
 * @param end Ending color of gradient
 */
void gfx_drawRoundedRectangleGradient(struct gfx_context *ctx, gfx_rect_t *rect, int radius, uint8_t type, gfx_color_t start, gfx_color_t end);

#endif
