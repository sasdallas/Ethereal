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

#if (defined(__i386__) || defined(__x86_64__)) && !defined(__NO_SSE)
#include <immintrin.h>
#endif

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

    sprite->alpha = SPRITE_ALPHA_BLEND;

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
    rewind(file); // Reset the file pointer to the beginning

    if (buffer[0] == 'B' && buffer[1] == 'M') {
        // Bitmap image
        return gfx_loadSpriteBMP(sprite, file);
    }

    // Unknown
    return 1;
}

/**
 * @brief Render a rectangle from a sprite
 * @param ctx Graphics context
 * @param sprite The sprite to render
 * @param rect A (relative-to-sprite) rectangle
 * @param x X coordinate of the sprite
 * @param y Y coordinate of the sprite
 */
int gfx_renderSpriteRegion(gfx_context_t *ctx, sprite_t *sprite, gfx_rect_t *rect, int x, int y) {
    // Calculate bounds of sprite
    uint32_t _left = GFX_MAX(x, 0);
    uint32_t _top = GFX_MAX(y, 0);
    uint32_t _right = GFX_MIN(x + sprite->width, GFX_WIDTH(ctx) - 1);
    uint32_t _bottom = GFX_MIN(y + sprite->height, GFX_HEIGHT(ctx) - 1);

    // Solid alpha?
    if (sprite->alpha == SPRITE_ALPHA_SOLID) {
        // Render normally like plebeians
        // uint8_t *tgt = (uint8_t*)&GFX_PIXEL(ctx, rect->x + x, y + rect->y);
        // uint8_t *src = (uint8_t*)&SPRITE_PIXEL(sprite, rect->x, rect->y);
        for (uint32_t _y = rect->y; _y < rect->y + rect->height; _y++) {
            memcpy(&GFX_PIXEL(ctx, rect->x + x, y + _y), &SPRITE_PIXEL(sprite, rect->x, _y), rect->width * 4);

            // memcpy(tgt, src, rect->width*4);
            // tgt += ctx->pitch;
            // src += sprite->width;
        }

        return 0;
    }

    // Calculate SSE masks
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__NO_SSE)
    __m128i mask00ff = _mm_set1_epi16(0x00FF);
    __m128i mask0080 = _mm_set1_epi16(0x0080);
    __m128i mask0101 = _mm_set1_epi16(0x0101);
#endif

    for (uint32_t _y = rect->y; _y < rect->y + rect->height; _y++) {
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__NO_SSE)
        (void)_left;

        // Use SSE to alpha blend quick
        // !!!: STOLEN: https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/lib/graphics.c
        if (y + _y < _top) continue;
        if (y + _y > _bottom) break;

        // Ensure alignment on the graphics context
        uint32_t _x = rect->x;
        for (; _x < rect->x + rect->width && _x + x <= _right; _x++) {
            uint32_t *pix = &GFX_PIXEL(ctx, _x + x, _y + y);
            if (!((uintptr_t)pix & 15)) break;
            *pix = gfx_alphaBlend(SPRITE_PIXEL(sprite, _x, _y), *pix);
        }

        // Now actually do the SSE drawing
        for (; _x < rect->x + rect->width - 3 && _x + x + 3 <= _right; _x += 4) {
            __m128i d = _mm_loadu_si128((void*)&GFX_PIXEL(ctx, x + _x, y + _y));
            __m128i s = _mm_loadu_si128((void*)&SPRITE_PIXEL(sprite, _x, _y));

            __m128i d_l, d_h;
            __m128i s_l, s_h;

            // unpack destination
            d_l = _mm_unpacklo_epi8(d, _mm_setzero_si128());
            d_h = _mm_unpackhi_epi8(d, _mm_setzero_si128());

            // unpack source
            s_l = _mm_unpacklo_epi8(s, _mm_setzero_si128());
            s_h = _mm_unpackhi_epi8(s, _mm_setzero_si128());

            __m128i a_l, a_h;
            __m128i t_l, t_h;

            // extract source alpha RGBA → AAAA
            a_l = _mm_shufflehi_epi16(_mm_shufflelo_epi16(s_l, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3));
            a_h = _mm_shufflehi_epi16(_mm_shufflelo_epi16(s_h, _MM_SHUFFLE(3,3,3,3)), _MM_SHUFFLE(3,3,3,3));

            // negate source alpha
            t_l = _mm_xor_si128(a_l, mask00ff);
            t_h = _mm_xor_si128(a_h, mask00ff);

            // apply source alpha to destination
            d_l = _mm_mulhi_epu16(_mm_adds_epu16(_mm_mullo_epi16(d_l,t_l),mask0080),mask0101);
            d_h = _mm_mulhi_epu16(_mm_adds_epu16(_mm_mullo_epi16(d_h,t_h),mask0080),mask0101);

            // combine source and destination
            d_l = _mm_adds_epu8(s_l,d_l);
            d_h = _mm_adds_epu8(s_h,d_h);

            // pack low + high and write back to memory
            _mm_storeu_si128((void*)&GFX_PIXEL(ctx, x + _x, y + _y), _mm_packus_epi16(d_l,d_h));
        }

        // Final row
        for (; _x < rect->x + rect->width && x + _x <= _right; _x++) {
            uint32_t *pix = &GFX_PIXEL(ctx, _x + x, _y + y);
            *pix = gfx_alphaBlend(SPRITE_PIXEL(sprite, _x, _y), *pix);
        }
#else
        // Render normally like plebeians
        for (uint32_t _x = rect->x; _x < rect->x + rect->width; _x++) {
            if (x + _x < _left || x + _x > _right || y + _y < _top || y + _y > _bottom) continue;
            GFX_PIXEL(ctx, _x + x, _y + y) = gfx_alphaBlend(SPRITE_PIXEL(sprite, _x, _y), GFX_PIXEL(ctx, _x + x, _y + y));
        }
#endif
    }
    

    return 0;
}

/**
 * @brief Render a sprite with an alpha vector applied
 * @param ctx The context to render the sprite in
 * @param sprite The sprite to render
 * @param x The X to render at
 * @param y The Y to render at
 * @param alpha The alpha vector to use
 */
int gfx_renderSpriteAlpha(struct gfx_context *ctx, sprite_t *sprite, int x, int y, uint8_t alpha) {
    gfx_rect_t full_rect = { .x = 0, .y = 0, .width = sprite->width, .height = sprite->height };


    uint32_t _left = GFX_MAX(x, 0);
    uint32_t _top = GFX_MAX(y, 0);
    uint32_t _right = GFX_MIN(x + sprite->width, GFX_WIDTH(ctx) - 1);
    uint32_t _bottom = GFX_MIN(y + sprite->height, GFX_HEIGHT(ctx) - 1);

    for (uint32_t _y = 0; _y < full_rect.height; _y++) {
        for (uint32_t _x = 0; _x < full_rect.width; _x++) {
            if (x + _x < _left || x + _x > _right || y + _y < _top || y + _y > _bottom) continue;
            uint32_t *pix = &GFX_PIXEL(ctx, x + _x, y + _y);
            uint32_t src_pixel = SPRITE_PIXEL(sprite, _x, _y);

            // Apply alpha blending
            // TODO: Maybe add some SSE code?
            uint8_t r = __gfx_premultiply_add_alpha_channel(src_pixel, R, alpha);
            uint8_t g = __gfx_premultiply_add_alpha_channel(src_pixel, G, alpha);
            uint8_t b = __gfx_premultiply_add_alpha_channel(src_pixel, B, alpha);
            uint8_t a = __gfx_premultiply_add_alpha_channel(src_pixel, A, alpha);

            *pix = gfx_alphaBlend(GFX_RGBA(r,g,b,a), *pix);
        }
    }

    return 0;
}

/**
 * @brief Render a sprite to a specific set of coordinates
 * @param sprite The sprite to render
 * @param x The X to render at
 * @param y The Y to render at
 */
int gfx_renderSprite(gfx_context_t *ctx, sprite_t *sprite, int x, int y) {
    gfx_rect_t full_rect = { .x = 0, .y = 0, .width = sprite->width, .height = sprite->height };
    return gfx_renderSpriteRegion(ctx, sprite, &full_rect, x, y);
}

/**
 * @brief Render a scaled sprite
 * @param ctx The context to render the sprite in
 * @param sprite The sprite to render
 * @param scaled The scaled rectangle to render in
 */
int gfx_renderSpriteScaled(gfx_context_t *ctx, sprite_t *sprite, gfx_rect_t scaled) {
    // See this article for a good picture on bilinear interpolation: https://meghal-darji.medium.com/implementing-bilinear-interpolation-for-image-resizing-357cbb2c2722
    for (size_t y = 0; y < scaled.height; y++) {
        float src_y = (float)y * sprite->height / scaled.height;
        size_t y0 = (size_t)src_y;
        size_t y1 = GFX_MIN(y0 + 1, sprite->height - 1);
        float y_lerp = src_y - y0;

        for (size_t x = 0; x < scaled.width; x++) {
            float src_x = (float)x * sprite->width / scaled.width;
            size_t x0 = (size_t)src_x;
            size_t x1 = GFX_MIN(x0 + 1, sprite->width - 1);
            float x_lerp = src_x - x0;

            uint32_t c00 = sprite->bitmap[y0 * sprite->width + x0];
            uint32_t c01 = sprite->bitmap[y0 * sprite->width + x1];
            uint32_t c10 = sprite->bitmap[y1 * sprite->width + x0];
            uint32_t c11 = sprite->bitmap[y1 * sprite->width + x1];
            
            // !!!
            uint8_t r = (1 - y_lerp) * ((1 - x_lerp) * GFX_RGB_R(c00) + x_lerp * GFX_RGB_R(c01)) +
                        y_lerp * ((1 - x_lerp) * GFX_RGB_R(c10) + x_lerp * GFX_RGB_R(c11));
            uint8_t g = (1 - y_lerp) * ((1 - x_lerp) * GFX_RGB_G(c00) + x_lerp * GFX_RGB_G(c01)) +
                        y_lerp * ((1 - x_lerp) * GFX_RGB_G(c10) + x_lerp * GFX_RGB_G(c11));
            uint8_t b = (1 - y_lerp) * ((1 - x_lerp) * GFX_RGB_B(c00) + x_lerp * GFX_RGB_B(c01)) +
                        y_lerp * ((1 - x_lerp) * GFX_RGB_B(c10) + x_lerp * GFX_RGB_B(c11));
            uint8_t a = (1 - y_lerp) * ((1 - x_lerp) * GFX_RGB_A(c00) + x_lerp * GFX_RGB_A(c01)) +
                        y_lerp * ((1 - x_lerp) * GFX_RGB_A(c10) + x_lerp * GFX_RGB_A(c11));

            GFX_PIXEL(ctx, scaled.x + x, scaled.y + y) = GFX_RGBA(r, g, b, a);
        }
    }

    return 0;
}

/**
 * @brief Destroy sprite
 * @param sp The sprite to destroy
 */
void gfx_destroySprite(sprite_t *sp) {
    if (sp->bitmap) free(sp->bitmap);
    free(sp);
}