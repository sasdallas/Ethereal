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
#include <stddef.h>
#include <stdio.h>

/**** DEFINITIONS ****/

#define SPRITE_PIXEL(sprite, x, y) (sprite->bitmap[sprite->width * (y) + (x)])

/**** TYPES ****/

typedef struct _sprite {
    size_t width;           // Width
    size_t height;          // Height
    uint32_t *bitmap;       // Bitmap
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
 * @brief Render a large sprite in all available holes on the context
 * This is only really used for background drawing.
 */
int gfx_renderSpriteInTheHoles(struct gfx_context *ctx, sprite_t *sprite);

/**
 * @brief Render a scaled sprite
 * @param ctx The context to render the sprite in
 * @param sprite The sprite to render
 * @param scaled The scaled rectangle to render in
 */
int gfx_renderSpriteScaled(struct gfx_context *ctx, sprite_t *sprite, gfx_rect_t scaled);

#endif