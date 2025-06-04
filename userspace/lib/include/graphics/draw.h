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

/**** TYPES ****/

/* Graphics color */
typedef uint32_t gfx_color_t;

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

#endif
