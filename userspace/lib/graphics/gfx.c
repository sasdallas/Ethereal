/**
 * @file userspace/lib/graphics/gfx.c
 * @brief Ethereal graphics library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/gfx/video.h>
#include <graphics/gfx.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Initialize fullscreen graphics
 * @param flags The flags to use when creating the graphics context
 * @returns A pointer to a new graphics context
 */
gfx_context_t *gfx_createFullscreen(int flags) {
    gfx_context_t *ctx = malloc(sizeof(gfx_context_t));
    ctx->fb_fd = open("/device/fb0", O_RDONLY);

    // Did we get a handle to the context device?
    if (ctx->fb_fd < 0) {
        free(ctx);
        return NULL;
    }

    // Yes, get some information
    video_info_t info;
    if (ioctl(ctx->fb_fd, IO_VIDEO_GET_INFO, &info) < 0) {
        // Uh oh
        free(ctx);
        return NULL;
    }

    ctx->ftlib = NULL;
    ctx->ft_initialized = 0;
    ctx->width = info.screen_width;
    ctx->height = info.screen_height;
    ctx->bpp = info.screen_bpp;
    ctx->pitch = info.screen_pitch;
    ctx->flags = flags;
    ctx->clip = NULL;
    ctx->clip_last = NULL;

    // mmap() the video interface in
    size_t bufsz = (info.screen_height * info.screen_pitch);
    ctx->buffer = mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fb_fd, 0);
    if (ctx->buffer == MAP_FAILED) {
        // We failed to map the buffer in...
        perror("mmap");

        free(ctx);
        return NULL;
    }

    // If the user wants to, make a backbuffer
    if (!(flags & CTX_NO_BACKBUFFER)) {
        ctx->backbuffer = malloc(GFX_SIZE(ctx));
    }
    return ctx;
}

/**
 * @brief Initialize graphics for a specific buffer, width, height, etc.
 * @param flags Flags of the context
 * @param buffer The buffer to use
 * @param width The width of the buffer
 * @param height The height of the buffer
 * @returns New graphics object 
 */
gfx_context_t *gfx_createContext(int flags, uint8_t *buffer, size_t width, size_t height) {
    gfx_context_t *ctx = malloc(sizeof(gfx_context_t));
    ctx->ftlib = NULL;
    ctx->ft_initialized = 0;
    ctx->flags = flags;
    ctx->width = width;
    ctx->height = height;
    ctx->bpp = 32; // TODO: Change?
    ctx->pitch = width * sizeof(uint32_t); // TODO: Change?
    ctx->clip = NULL;
    ctx->clip_last = NULL;
    ctx->fb_fd = -1;
    ctx->buffer = buffer;

    if (!(flags & CTX_NO_BACKBUFFER)) {
        ctx->backbuffer = malloc(width * height * (ctx->bpp/8));
    } else {
        ctx->backbuffer = NULL;
    }

    return ctx;
}

/**
 * @brief Render what is in the backbuffer to the main buffer
 * @param ctx The context to render
 */
void gfx_render(gfx_context_t *ctx) {
    if (!ctx) return;
    if (ctx->flags & CTX_NO_BACKBUFFER) return;

    if (ctx->clip) {
        gfx_clip_t *clip = ctx->clip;
        while (clip) {
            // Copy this clip
            for (uint32_t y = clip->rect.y; y <= clip->rect.y + clip->rect.height; y++) {
                memcpy(&((char*)ctx->buffer)[y * GFX_PITCH(ctx) + (clip->rect.x * 4)], &((char*)ctx->backbuffer)[y * GFX_PITCH(ctx) + (clip->rect.x * 4)], (clip->rect.width+1) * 4);
            }

            clip = clip->next;
        }
    } else {

        for (uint32_t y = 0; y < ctx->height; y++) {
            memcpy(ctx->buffer + (y*GFX_PITCH(ctx)), ctx->backbuffer + (y*GFX_PITCH(ctx)), GFX_WIDTH(ctx) * 4);
        }
        // memcpy(ctx->buffer, ctx->backbuffer, GFX_SIZE(ctx));
    }
}

/**
 * @brief Clear the buffer with a specific color
 * @param ctx The context to clear the buffer of
 * @param color The color to use while clearing
 */
void gfx_clear(gfx_context_t *ctx, gfx_color_t color) {
    if (!ctx) return;

    if (ctx->flags & CTX_NO_BACKBUFFER) {
        for (uint32_t y = 0; y < ctx->height; y++) {
            for (uint32_t x = 0; x < ctx->width; x++) {
                GFX_PIXEL_REAL(ctx, x, y) = color;
            }
        }
    } else {
        for (uint32_t y = 0; y < ctx->height; y++) {
            for (uint32_t x = 0; x < ctx->width; x++) {
                GFX_PIXEL(ctx, x, y) = color;
            }
        }
    }
}

/**
 * @brief Create a new clip in the graphics context
 * @param ctx The context to use
 * @param x Starting X of the clip
 * @param y Starting Y of the clip
 * @param width Width of the clip
 * @param height Height of the clip
 */
void gfx_createClip(gfx_context_t *ctx, uint32_t x, uint32_t y, size_t width, size_t height) {
    gfx_clip_t *clip = malloc(sizeof(gfx_clip_t));
    clip->rect.x = x;
    clip->rect.y = y;
    clip->rect.width = width;
    clip->rect.height = height;
     
    if (ctx->clip) {
        ctx->clip_last->next = clip;
        clip->prev = ctx->clip_last;
        clip->next = NULL;
        ctx->clip_last = clip;
    } else {
        ctx->clip = clip;
        ctx->clip_last = clip;
        clip->prev = NULL;
        clip->next = NULL;
    }
}

/**
 * @brief Determine whether something is in a clip
 * @param ctx The context to use
 * @param x Starting X
 * @param y Starting Y
 * @param width Width
 * @param height Height
 * @warning This is slow...
 */
int gfx_isInClip(gfx_context_t *ctx, uint32_t x, uint32_t y, size_t width, size_t height) {
    if (!ctx->clip) return 1; // Not using clips

    gfx_clip_t *clip = ctx->clip;
    while (clip) {
        if (clip->rect.x <= x && clip->rect.y <= y && clip->rect.y + clip->rect.height >= y + height && clip->rect.width + clip->rect.x >= x + width) {
            return 1;
        }

        clip = clip->next;
    }

    return 0;
}

/**
 * @brief Reset all clips
 * @param ctx The context to reset
 */
void gfx_resetClips(gfx_context_t *ctx) {
    gfx_clip_t *clip = ctx->clip;
    gfx_clip_t *prev = clip;

    while (clip) {
        prev = clip;
        clip = clip->next;
        free(prev);
    }

    ctx->clip = NULL;
    ctx->clip_last = NULL;
}

void gfx_drawClips(gfx_context_t *ctx) {
    gfx_clip_t *clip = ctx->clip;
    while (clip) {
        gfx_drawRectangle(ctx, &clip->rect, GFX_RGB(255, 0, 0));
        clip = clip->next;
    }
}