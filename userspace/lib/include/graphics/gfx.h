/**
 * @file userspace/lib/include/graphics/gfx.h
 * @brief Graphics library header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef _GRAPHICS_GFX_H
#define _GRAPHICS_GFX_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <graphics/draw.h>
#include <graphics/sprite.h>

/**** DEFINITIONS ****/

/* Graphics context flags */
#define CTX_DEFAULT                 0
#define CTX_NO_BACKBUFFER           1

/**** TYPES ****/

/**
 * @brief Graphics clip
 */
typedef struct gfx_clip {
    gfx_rect_t rect;

    struct gfx_clip *next;  // Next graphics clip
    struct gfx_clip *prev;  // Previous graphics clip
} gfx_clip_t;



/**
 * @brief Main graphics context
 */
typedef struct gfx_context {
    int flags;              // Flags of the graphics context
    void *buffer;           // Pointer to the graphics buffer
    void *backbuffer;       // Pointer to the graphics backbuffer
    int fb_fd;              // Framebuffer file descriptor, if used

    gfx_clip_t *clip;       // Clip
    gfx_clip_t *clip_last;  // Last clip
                            
    size_t width;           // Width of the buffer
    size_t height;          // Height of the buffer
    size_t bpp;             // Bits per pixel of the buffer
    size_t pitch;           // Pitch of the buffer
} gfx_context_t;

/**** MACROS ****/

/* Color manipulation */
#define GFX_RGB_A(color) ((color & 0xFF000000) / 0x1000000)
#define GFX_RGB_R(color) ((color & 0x00FF0000) / 0x10000)
#define GFX_RGB_G(color) ((color & 0x0000FF00) / 0x100)
#define GFX_RGB_B(color) ((color & 0x000000FF))
#define GFX_RGBA(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | (b))
#define GFX_RGB(r, g, b) GFX_RGBA(r, g, b, 255)

/* Easy ways to get context variables */
#define GFX_BACKBUFFER(ctx) (ctx->backbuffer)
#define GFX_BUFFER(ctx) (ctx->buffer)
#define GFX_WIDTH(ctx) (ctx->width)
#define GFX_HEIGHT(ctx) (ctx->height)
#define GFX_BPP(ctx) (ctx->bpp)
#define GFX_PITCH(ctx) (ctx->pitch)
#define GFX_SIZE(ctx) (ctx->height * ctx->pitch)

/* Pixel setting */
#define GFX_PIXEL(ctx, x, y) (*(uint32_t*)(ctx->backbuffer + (((y) * GFX_PITCH(ctx)) + ((x) * (GFX_BPP(ctx)/8)))))
#define GFX_PIXEL_REAL(ctx, x, y) (*(uint32_t*)(ctx->buffer + (((y) * GFX_PITCH(ctx)) + ((x) * (GFX_BPP(ctx)/8)))))

/**** FUNCTIONS ****/

/**
 * @brief Initialize fullscreen graphics
 * @param flags The flags to use when creating the graphics context
 * @returns A pointer to a new graphics context
 */
gfx_context_t *gfx_createFullscreen(int flags);

/**
 * @brief Render what is in the backbuffer to the main buffer
 * @param ctx The context to render
 */
void gfx_render(gfx_context_t *context);

/**
 * @brief Clear the buffer with a specific color
 * @param ctx The context to clear the buffer of
 * @param color The color to use while clearing
 */
void gfx_clear(gfx_context_t *ctx, gfx_color_t color);

/**
 * @brief Create a new clip in the graphics context
 * @param ctx The context to use
 * @param x Starting X of the clip
 * @param y Starting Y of the clip
 * @param width Width of the clip
 * @param height Height of the clip
 */
void gfx_createClip(gfx_context_t *ctx, uint32_t x, uint32_t y, size_t width, size_t height);

/**
 * @brief Determine whether something is in a clip
 * @param ctx The context to use
 * @param x Starting X
 * @param y Starting Y
 * @param width Width
 * @param height Height
 * @warning This is slow...
 */
int gfx_isInClip(gfx_context_t *ctx, uint32_t x, uint32_t y, size_t width, size_t height);

/**
 * @brief Reset all clips
 * @param ctx The context to reset
 */
void gfx_resetClips(gfx_context_t *ctx);

/**
 * @brief Alpha blend two colors together
 * @param top The topmost color
 * @param bottom The bottom color
 */
gfx_color_t gfx_alphaBlend(gfx_color_t top, gfx_color_t bottom);

#endif