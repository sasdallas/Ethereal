/**
 * @file userspace/celestialv2/renderer_generic.c
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


void render_request(render_request_t *upd) {
    if (!upd->win) {
        gfx_drawRectangleFilled(RENDERER->ctx, &upd->rect, GFX_RGB(0, 0, 0));
        return;
    }
    
    if (upd->state == WINDOW_STATE_OPENING) {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = SPRITE_ALPHA_BLEND
        };

        double tdiff = (double)upd->anim_time / (double)125000;
        double scale = 0.9 + tdiff * (1.0 - 0.9); // Starting scale at 90%, scale up to 100%
        double off_x = (upd->win->width - (upd->win->width * scale)) / 2.0f;
        double off_y = (upd->win->height - (upd->win->height * scale)) / 2.0f;
        gfx_mat2x3_t mat = gfx_mat2x3_identity();
        gfx_mat2x3_translate(&mat, off_x + upd->x, off_y + upd->y);
        gfx_mat2x3_scale(&mat, scale, scale);

        scale = 0.0 + tdiff * (1.0 - 0.0);
        gfx_renderSpriteTransform(RENDERER->ctx, &sp, &mat, 255*scale);
    } else if (upd->state == WINDOW_STATE_CLOSING) {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = SPRITE_ALPHA_BLEND
        };

        double tdiff = (double)upd->anim_time / (double)125000;
        double scale = 1.0 + tdiff * (0.9 - 1.0); // Starting scale at 90%, scale up to 100%
        double off_x = (upd->win->width - (upd->win->width * scale)) / 2.0f;
        double off_y = (upd->win->height - (upd->win->height * scale)) / 2.0f;
        gfx_mat2x3_t mat = gfx_mat2x3_identity();
        gfx_mat2x3_translate(&mat, off_x + upd->x, off_y + upd->y);
        gfx_mat2x3_scale(&mat, scale, scale);

        scale = 1.0 + tdiff * (0.0 - 1.0);
        gfx_renderSpriteTransform(RENDERER->ctx, &sp, &mat, 255*scale);
    } else {
        sprite_t sp = {
            .width = upd->win->width,
            .height = upd->win->height,
            .bitmap = (uint32_t*)upd->win->buffer,
            .alpha = (upd->win->flags & CELESTIAL_WINDOW_FLAG_SOLID) ? SPRITE_ALPHA_SOLID : SPRITE_ALPHA_BLEND
        };

        if (upd->state == WINDOW_STATE_CLOSED) {
            window_release(upd->win);
            return;
        }
        
        // TRACE_DEBUG("Render sprite %dx%d bitmap %p\n", sp.width, sp.height, sp.bitmap);
        gfx_renderSpriteRegion(RENDERER->ctx, &sp, &upd->rect, upd->x, upd->y);
    }

    window_release(upd->win);
}
