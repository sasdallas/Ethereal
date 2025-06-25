/**
 * @file userspace/lib/graphics/sprite.c
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

#include <graphics/gfx.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a new sprite object
 * @param width The width of the sprite
 * @param height The height of the sprite
 * 
 * @note You can call this as @c gfx_createSprite(0,0) to make an empty sprite.
 */
sprite_t *gfx_createSprite(size_t width, size_t height) {
    sprite_t *sprite = malloc(sizeof(sprite_t));

    sprite->width = width;
    sprite->height = height;

    if (!width || !height) {
        sprite->bitmap = NULL;
    } else {
        sprite->bitmap = malloc(sizeof(uint32_t) * width * height);
    }

    return sprite;
}

static inline gfx_color_t __premultiply(gfx_color_t color) {
    uint16_t a = GFX_RGB_A(color);
    uint16_t r = GFX_RGB_R(color) * a / 255;
    uint16_t g = GFX_RGB_G(color) * a / 255;
    uint16_t b = GFX_RGB_B(color) * a / 255;
    return GFX_RGBA(r, g, b, a);
}

/**
 * @brief Load a bitmap object
 */
static int gfx_loadSpriteBMP(sprite_t *sprite, FILE *file) {
    // !!!: THIS CODE IS REWRITTEN FROM ToaruOS - I promise I come back and fix it later
    // !!!: EXACT SOURCE OF THIS FUNCTION: https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/lib/graphics.c
    // Read the full file in
    size_t size = 0;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size);
    memset(buffer, 0, size);
    if (fread(buffer, size, 1, file) < 1) {
        return 1;
    }


    signed int *bufferi = (signed int *)((uintptr_t)buffer + 2);

    // Setup everything
    sprite->width = bufferi[4];
    sprite->height = bufferi[5];

    // Get BPP and row width
    uint16_t bpp = bufferi[6] / 0x10000;
    uint32_t row_width = (bpp * sprite->width + 31) / 32 * 4;

    // Now setup sprite parameters
    if (sprite->bitmap) free(sprite->bitmap);
    sprite->bitmap = malloc(sizeof(uint32_t) * sprite->width * sprite->height);
    memset(sprite->bitmap, 0, sizeof(uint32_t) * sprite->width * sprite->height);

    // Build the bitmap
    int alpha_after = ((unsigned char *)&bufferi[13])[2] == 0xFF;

    #define _BMP_A 0x1000000
    #define _BMP_R 0x1
    #define _BMP_G 0x100
    #define _BMP_B 0x10000

    // Parse! 
    size_t i = bufferi[2];
    for (uint16_t y = 0; y < sprite->height; y++) {
        for (uint16_t x = 0; x < sprite->width; x++) {
            if (i > size) goto _done;

            uint32_t color;
            if (bpp == 24) {
                color =	(buffer[i   + 3 * x] & 0xFF) +
							(buffer[i+1 + 3 * x] & 0xFF) * 0x100 +
							(buffer[i+2 + 3 * x] & 0xFF) * 0x10000 + 0xFF000000;
                color = __premultiply(color);
            } else if (bpp == 32 && alpha_after == 0) {
                color =	(buffer[i   + 4 * x] & 0xFF) * _BMP_A +
                        (buffer[i+1 + 4 * x] & 0xFF) * _BMP_R +
                        (buffer[i+2 + 4 * x] & 0xFF) * _BMP_G +
                        (buffer[i+3 + 4 * x] & 0xFF) * _BMP_B;
                        color = __premultiply(color);
            } else if (bpp == 32 && alpha_after == 1) {
				color =	(buffer[i   + 4 * x] & 0xFF) * _BMP_R +
                    (buffer[i+1 + 4 * x] & 0xFF) * _BMP_G +
                    (buffer[i+2 + 4 * x] & 0xFF) * _BMP_B +
                    (buffer[i+3 + 4 * x] & 0xFF) * _BMP_A;
                    color = __premultiply(color);
            } else {
                color = GFX_RGB(255, 0, 0);
            }

            sprite->bitmap[(sprite->height - y - 1) * sprite->width + x] = color;
        }

        i += row_width;
    }

_done:
    free(buffer);
    return 0;
}

/**
 * @brief Load an image into the sprite object
 * @param sprite The sprite to load the image file into
 * @param file The image file to load into the sprite object
 * @returns 0 on success 
 */
int gfx_loadSprite(sprite_t *sprite, FILE *file) {
    if (!sprite || !file) return 1;

    // Try to identify the file
    uint8_t buffer[4];
    if (fread(buffer, 4, 1, file) < 1) {
        return 1;
    }

    if (buffer[0] == 'B' && buffer[1] == 'M') {
        // Bitmap image
        return gfx_loadSpriteBMP(sprite, file);
    }

    // Unknown
    return 1;
}

/**
 * @brief Render a sprite to a specific set of coordinates
 * @param sprite The sprite to render
 * @param x The X to render at
 * @param y The Y to render at
 */
int gfx_renderSprite(gfx_context_t *ctx, sprite_t *sprite, int x, int y) {
    // Calculate bounds of sprites
    int32_t _left = GFX_MAX(x, 0);
    int32_t _top = GFX_MAX(y, 0);
    int32_t _right = GFX_MIN(x + sprite->width, GFX_WIDTH(ctx) - 1);
    int32_t _bottom = GFX_MIN(y + sprite->height, GFX_HEIGHT(ctx) - 1);

    for (uint16_t _y = 0; _y < sprite->height; _y++) {
        if (y + _y < _top) continue;
        if (y + _y > _bottom) break;

        for (uint16_t _x = 0; _x < sprite->width; _x++) {
            // Are we in bounds?
            if (x + _x < _left || x + _x > _right || y + _y < _top || y + _y > _bottom) continue;

            // Place pixel
            GFX_PIXEL(ctx, x + _x, y + _y) = gfx_alphaBlend(SPRITE_PIXEL(sprite, _x, _y), GFX_PIXEL(ctx, x + _x, y + _y));
        }
    }

    return 0;
}

/**
 * @brief Render a large sprite in all available holes on the context
 * This is only really used for background drawing.
 */
int gfx_renderSpriteInTheHoles(gfx_context_t *ctx, sprite_t *sprite) {
    gfx_clip_t *clip = ctx->clip;
    while (clip) {

        // Copy!
        for (uint16_t y = clip->rect.y; y < clip->rect.y + clip->rect.height; y++) {
            // memcpy(&((char*)ctx->backbuffer)[y * GFX_PITCH(ctx) + (clip->rect.x * 4)], &((char*)sprite->bitmap)[y * sprite->width + (clip->rect.x * 4)], (clip->rect.width+1) * 4);
            for (uint16_t x = clip->rect.x; x < clip->rect.x + clip->rect.width; x++) {
                GFX_PIXEL(ctx, x, y) = SPRITE_PIXEL(sprite, x, y);
            }
        }

        clip = clip->next;
    }

    return 0;
}