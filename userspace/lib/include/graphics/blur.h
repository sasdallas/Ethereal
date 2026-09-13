/**
 * @file userspace/lib/include/graphics/blur.h
 * @brief Gaussian blur support
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _GRAPHICS_BLUR_H
#define _GRAPHICS_BLUR_H

/**** INCLUDES ****/
#include <stddef.h>
#include <stdint.h>
#include <graphics/color.h>

/**** TYPES ****/

typedef struct gfx_blur {
    size_t max_width;
    unsigned int max_radius;
    unsigned int radius;
    uint64_t weight_total;
    uint32_t *kernel;
    gfx_color_t *rows;
} gfx_blur_t;

struct gfx_context;
struct gfx_rect;

/**** FUNCTIONS ****/

/**
 * @brief Create a Gaussian blur context
 * @param max_width Maximum width that this context can blur
 * @param max_radius Maximum supported blur radius
 * @returns Blur object
 */
gfx_blur_t *gfx_createBlur(size_t max_width, unsigned int max_radius);

/**
 * @brief Configure a Gaussian blur context
 * @param blur Blur context
 * @param radius Blur radius
 * @param sigma Gaussian standard deviation
 */
void gfx_configureBlur(gfx_blur_t *blur, unsigned int radius, float sigma);

/**
 * @brief Blur a region of a graphics context in place
 * @param ctx Graphics context
 * @param blur Blur object
 * @param rect Region to blur
 */
void gfx_blurContextRegion(struct gfx_context *ctx, gfx_blur_t *blur, struct gfx_rect *rect);

/**
 * @brief Destroy a blur context
 * @param blur Blur object
 */
void gfx_destroyBlur(gfx_blur_t *blur);

#endif
