/**
 * @file userspace/lib/graphics/blur.c
 * @brief Double-pass box blur
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <graphics/gfx.h>

typedef struct blur_sum {
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
} blur_sum_t;

/**
 * @brief Helper to add sums
 */
static inline void gfx_addBlurSum(blur_sum_t *out, gfx_color_t color) {
    out->r += GFX_RGB_R(color);
    out->g += GFX_RGB_G(color);
    out->b += GFX_RGB_B(color);
    out->a += GFX_RGB_A(color);
}

/**
 * @brief Helper to average sums
 */
static inline gfx_color_t gfx_averageBlurSum(blur_sum_t *sum, int num_colors) {
    uint32_t rounding = num_colors / 2;
    return GFX_RGBA(
        (sum->r + rounding) / num_colors,
        (sum->g + rounding) / num_colors,
        (sum->b + rounding) / num_colors,
        (sum->a + rounding) / num_colors
    );
}

/**
 * @brief Helper to replace a color in the sum
 */
static inline void gfx_replaceBlurSum(blur_sum_t *out, gfx_color_t old, gfx_color_t new) {
    out->r += GFX_RGB_R(new) - GFX_RGB_R(old);
    out->g += GFX_RGB_G(new) - GFX_RGB_G(old);
    out->b += GFX_RGB_B(new) - GFX_RGB_B(old);
    out->a += GFX_RGB_A(new) - GFX_RGB_A(old);
}


/**
 * @brief Horizontal pass
 */
static void gfx_horizontalBlur(gfx_context_t *ctx, gfx_blur_t *blur, size_t left, size_t width, gfx_color_t *out, blur_sum_t *vertical, size_t y) {
    uint32_t *buffer = (ctx->flags & CTX_NO_BACKBUFFER) ? ctx->buffer : ctx->backbuffer;
    buffer = (uint32_t*)((uint8_t*)buffer + (y * GFX_PITCH(ctx)));
    blur_sum_t sum = { 0 };
    int count = blur->radius * 2 + 1;

    // build the first window
    for (int dx = -(int)blur->radius; dx <= (int)blur->radius; dx++) {
        size_t x = gfx_clamp(left + dx, 0, GFX_WIDTH(ctx));
        gfx_addBlurSum(&sum, buffer[x]);
    }

    for (size_t x = 0; x < width; x++) {
        gfx_color_t color = gfx_averageBlurSum(&sum, count);

        if (vertical != NULL) {
            gfx_replaceBlurSum(&vertical[x], out[x], color);
        }

        out[x] = color;

        // slide the window across
        size_t to_remove = gfx_clamp(left + x - blur->radius, 0, GFX_WIDTH(ctx));
        size_t to_add = gfx_clamp(left + x + blur->radius + 1, 0, GFX_WIDTH(ctx));
        gfx_replaceBlurSum(&sum, buffer[to_remove], buffer[to_add]);
    }
} 

/**
 * @brief Create a box blur context
 * @param max_width Maximum width that this context can blur
 * @param max_radius Maximum supported blur radius
 * @returns Blur object
 */
gfx_blur_t *gfx_createBlur(size_t max_width, unsigned int max_radius) {
    gfx_blur_t *blur = malloc(sizeof(gfx_blur_t));
    size_t rows = max_radius * 2 + 1;
    blur->kernel = malloc(max_width * 4 * sizeof(uint32_t));
    blur->rows = malloc(max_width * rows * sizeof(uint32_t));
    blur->max_width = max_width;
    blur->max_radius = max_radius;
    blur->radius = 0;
    blur->weight_total = 1;

    return blur;
}


/**
 * @brief Configure a box blur context
 * @param blur Blur context
 * @param radius Blur radius
 * @param sigma Not used
 */
void gfx_configureBlur(gfx_blur_t *blur, unsigned int radius, float sigma) {
    if (radius > blur->max_radius) radius = blur->max_radius;

    blur->radius = radius;
    blur->weight_total = radius * 2 + 1;
}

/**
 * @brief Blur a region of a graphics context in place
 * @param ctx Graphics context
 * @param blur Blur object
 * @param rect Region to blur
 */
void gfx_blurContextRegion(gfx_context_t *ctx, gfx_blur_t *blur, gfx_rect_t *rect) {
    size_t left = GFX_MIN(rect->x, GFX_WIDTH(ctx));
    size_t top = GFX_MIN(rect->y, GFX_HEIGHT(ctx));
    size_t width = GFX_MIN(rect->width, GFX_WIDTH(ctx) - left);
    size_t height = GFX_MIN(rect->height, GFX_HEIGHT(ctx) - top);

    if (!width || !height) {
        return;
    }

    size_t nrows = blur->radius * 2 + 1;
    gfx_color_t *rows = blur->rows;
    blur_sum_t *vertical = (blur_sum_t*)blur->kernel;

    // first horizontal pass
    for (size_t row = 0; row < nrows; row++) {
        size_t src_y = gfx_clamp(top + row - blur->radius, 0, GFX_HEIGHT(ctx));
        gfx_horizontalBlur(ctx, blur, left, width, &rows[row * blur->max_width], NULL, src_y);
    }

    // build vertical sums
    memset(vertical, 0, sizeof(blur_sum_t) * width);
    for (size_t row = 0; row < nrows; row++) {
        gfx_color_t *src = &rows[row * blur->max_width];
        for (size_t x = 0; x < width; x++) {
            gfx_addBlurSum(&vertical[x], src[x]);
        }
    }

    // Final rows
    size_t ring = 0;
    for (size_t y = 0; y < height; y++) {
        gfx_color_t *dest;

        if (ctx->flags & CTX_NO_BACKBUFFER) {
            dest = &GFX_PIXEL_REAL(ctx, left, top + y);
        } else {
            dest = &GFX_PIXEL(ctx, left, top + y);
        }

        for (size_t x = 0; x < width; x++) {
            dest[x] = gfx_averageBlurSum(&vertical[x], nrows);
        }

        if (y+1 == height) break;

        size_t src_y = gfx_clamp(top + y + blur->radius + 1, 0, GFX_HEIGHT(ctx));
        gfx_color_t *row = &rows[ring * blur->max_width];

        gfx_horizontalBlur(ctx, blur, left, width, row, vertical, src_y);

        ring = (ring+1) % nrows;
    }
}

/**
 * @brief Destroy a blur context
 * @param blur Blur object
 */
void gfx_destroyBlur(gfx_blur_t *blur) {
    free(blur->kernel);
    free(blur->rows);
    free(blur);
}
