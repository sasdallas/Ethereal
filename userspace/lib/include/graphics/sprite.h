/**
 * @file userspace/lib/include/graphics/sprite.h
 * @brief Sprite code
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_SPRITE_H
#define _GRAPHICS_SPRITE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/matrix.h>
#include <stddef.h>
#include <stdio.h>

/**** DEFINITIONS ****/

#define SPRITE_PIXEL(sprite, x, y) (sprite->bitmap[sprite->width * (y) + (x)])

#define SPRITE_ALPHA_BLEND          0
#define SPRITE_ALPHA_SOLID          1

/**** TYPES ****/

typedef struct _sprite {
    size_t width;           // Width
    size_t height;          // Height
    uint32_t *bitmap;       // Bitmap
    unsigned char alpha;    // Alpha type
} sprite_t;

/**** MACROS ****/

#define GFX_MIN(a, b) (((a) > (b) ? (b) : (a)))
#define GFX_MAX(a, b) (((a) > (b) ? (a) : (b)))


/**** FUNCTIONS ****/

/**
 * @brief Create a new sprite object
 * @param width The width of the sprite
 * @param height The height of the sprite
 * 
 * @note You can call this as @c gfx_createSprite(0,0) to make an empty sprite.
 */
sprite_t *gfx_createSprite(size_t width, size_t height);

/**
 * @brief Load an image into the sprite object
 * @param sprite The sprite to load the image file into
 * @param file The image file to load into the sprite object
 * @returns 0 on success 
 */
int gfx_loadSprite(sprite_t *sprite, FILE *file);

/**
 * @brief Render a sprite to a specific set of coordinates
 * @param sprite The sprite to render
 * @param x The X to render at
 * @param y The Y to render at
 */
int gfx_renderSprite(struct gfx_context *ctx, sprite_t *sprite, int x, int y);

/**
 * @brief Render a scaled sprite
 * @param ctx The context to render the sprite in
 * @param sprite The sprite to render
 * @param scaled The scaled rectangle to render in
 */
int gfx_renderSpriteScaled(struct gfx_context *ctx, sprite_t *sprite, gfx_rect_t scaled);

/**
 * @brief Render a sprite with an alpha vector applied
 * @param ctx The context to render the sprite in
 * @param sprite The sprite to render
 * @param x The X to render at
 * @param y The Y to render at
 * @param alpha The alpha vector to use
 */
int gfx_renderSpriteAlpha(struct gfx_context *ctx, sprite_t *sprite, int x, int y, uint8_t alpha);

/**
 * @brief Render a rectangle from a sprite
 * @param ctx Graphics context
 * @param sprite The sprite to render
 * @param rect A (relative-to-sprite) rectangle
 * @param x X coordinate of the sprite
 * @param y Y coordinate of the sprite
 */
int gfx_renderSpriteRegion(struct gfx_context *ctx, sprite_t *sprite, gfx_rect_t *rect, int x, int y);

/**
 * @brief Draw a sprite using a transformation matrix
 * @param ctx The context to draw to
 * @param sprite The sprite to draw
 * @param matrix The matrix to use to draw the sprite
 * @param alpha The alpha vector with which to use the sprite drawing
 */
void gfx_renderSpriteTransform(struct gfx_context *ctx, sprite_t *sprite, gfx_mat2x3_t *matrix, uint8_t alpha);

/**
 * @brief Destroy sprite
 * @param sp The sprite to destroy
 */
void gfx_destroySprite(sprite_t *sp);

#endif