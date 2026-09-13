/**
 * @file userspace/celestial/renderer_generic.c
 * @brief Generic renderer functions
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "celestial.h"
#include <graphics/gfx.h>
#include <ethereal/celestial.h>

#define BLUR_RADIUS 8
#define BLUR_SIGMA 3.0f

static gfx_blur_t *render_blur = NULL;
bool render_blur_enable = true;

int renderer_initGeneric() {
    render_blur = gfx_createBlur(renderer_getWidth(), BLUR_RADIUS);
    gfx_configureBlur(render_blur, BLUR_RADIUS, BLUR_SIGMA);
    return 0;
}

void renderer_shutdownGeneric() {
    gfx_destroyBlur(render_blur);
}

void render_request(render_request_t *upd) {
    if (!upd->win) {
        gfx_drawRectangleFilled(RENDERER->ctx, &upd->rect, GFX_RGB(0, 0, 0));
        return;
    }

    bool is_resizing = (upd->win->state == WINDOW_STATE_RESIZING || upd->win->resize.oneoff);

    // !!! This is a penalty. Need to upgrade is_resizing to be better :(
    pthread_spin_lock(&upd->win->resize.resize_lck);

    if (is_resizing) {
        // A window that was resizing may have had its upd rect changed. Clamp it back
        gfx_rect_t *r = &upd->rect;
        wm_window_t *win = upd->win;
        if (r->x > (unsigned)win->width || r->y > (unsigned)win->height) {
            pthread_spin_unlock(&win->resize.resize_lck);
            return;
        }
        if (r->x + r->width > (unsigned)win->width) r->width = win->width - r->x;
        if (r->y + r->height > (unsigned)win->height) r->height = win->height - r->y;
    }

    if (upd->win->flags & CELESTIAL_WINDOW_FLAG_BLURRED && render_blur_enable) {
        int left = GFX_MAX(upd->x + (int)upd->rect.x, 0);
        int top = GFX_MAX(upd->y + (int)upd->rect.y, 0);
        int right = GFX_MIN(upd->x + (int)(upd->rect.x + upd->rect.width), (int)renderer_getWidth());
        int bottom = GFX_MIN(upd->y + (int)(upd->rect.y + upd->rect.height), (int)renderer_getHeight());

        if (right > left && bottom > top) {
            gfx_rect_t blur_rect = GFX_RECT(left, top, right - left, bottom - top);
            gfx_blurContextRegion(RENDERER->ctx, render_blur, &blur_rect);
        }
    }

    if (upd->state == WINDOW_STATE_OPENING) {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = SPRITE_ALPHA_BLEND
        };

        double tdiff = (double)upd->win->anim.anim_time / (double)125000;

        if (upd->win->flags & CELESTIAL_WINDOW_FLAG_FADE_ANIM) {
            double scale = 0.0 + tdiff * (1.0 - 0.0);
            gfx_renderSpriteAlpha(RENDERER->ctx, &sp, upd->x, upd->y, scale * 255);
        } else {
            double scale = 0.9 + tdiff * (1.0 - 0.9); // Starting scale at 90%, scale up to 100%
            double off_x = (upd->win->width - (upd->win->width * scale)) / 2.0f;
            double off_y = (upd->win->height - (upd->win->height * scale)) / 2.0f;
            gfx_mat2x3_t mat = gfx_mat2x3_identity();
            gfx_mat2x3_translate(&mat, off_x + upd->x, off_y + upd->y);
            gfx_mat2x3_scale(&mat, scale, scale);

            scale = 0.0 + tdiff * (1.0 - 0.0);
            gfx_renderSpriteTransform(RENDERER->ctx, &sp, &mat, 255*scale);
        }
    } else if (upd->state == WINDOW_STATE_CLOSING) {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = SPRITE_ALPHA_BLEND
        };

        double tdiff = (double)upd->win->anim.anim_time / (double)125000;

        if (upd->win->flags & CELESTIAL_WINDOW_FLAG_FADE_ANIM) {
            double scale = 1.0 + tdiff * (0.0 - 1.0);
            gfx_renderSpriteAlpha(RENDERER->ctx, &sp, upd->x, upd->y, scale * 255);
        } else {
            double scale = 1.0 + tdiff * (0.9 - 1.0); // Starting scale at 90%, scale up to 100%
            double off_x = (upd->win->width - (upd->win->width * scale)) / 2.0f;
            double off_y = (upd->win->height - (upd->win->height * scale)) / 2.0f;
            gfx_mat2x3_t mat = gfx_mat2x3_identity();
            gfx_mat2x3_translate(&mat, off_x + upd->x, off_y + upd->y);
            gfx_mat2x3_scale(&mat, scale, scale);

            scale = 1.0 + tdiff * (0.0 - 1.0);
            gfx_renderSpriteTransform(RENDERER->ctx, &sp, &mat, 255*scale);
        }
    } else {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = (upd->win->flags & CELESTIAL_WINDOW_FLAG_SOLID) ? SPRITE_ALPHA_SOLID : SPRITE_ALPHA_BLEND
        };

        if (upd->state == WINDOW_STATE_CLOSED) {
            window_release(upd->win);
            pthread_spin_unlock(&upd->win->resize.resize_lck);
            return;
        }
        
        gfx_renderSpriteRegion(RENDERER->ctx, &sp, &upd->rect, upd->x, upd->y);
    }
    
    // !!! Performance penalty to acquire lock every render
    pthread_spin_unlock(&upd->win->resize.resize_lck);

    window_release(upd->win);
}
