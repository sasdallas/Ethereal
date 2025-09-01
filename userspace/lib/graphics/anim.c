/**
 * @file userspace/lib/graphics/anim.c
 * @brief Ethereal graphics animations
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
#include <time.h>
#include <sys/time.h>

/**
 * @brief Get current time
 */
static uint64_t gfx_now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000UL + tv.tv_usec;
}

/**
 * @brief Tick animations
 * @param ctx The context to tick animations on
 */
void gfx_tickAnimations(struct gfx_context *ctx) {
    // Get the current time
    uint64_t now = gfx_now();

    if (ctx->animations) {
        node_t *an = ctx->animations->head;
        while (an) {
            gfx_anim_t *anim = (gfx_anim_t*)an->value;

            if (anim->state == GFX_ANIM_RUNNING) {
                if (now - anim->last_frame >= anim->delay) {
                    if (!anim->frame) {
                        if (anim->start) anim->start(ctx, anim);
                    
                        if (!(anim->flags & GFX_ANIM_FLAG_NO_SAVE)) {
                            // Create a saved context
                            anim->saved_chunk = gfx_createSprite(anim->sprite->width, anim->sprite->height);

                            for (unsigned i = 0; i < anim->saved_chunk->height; i++) {
                                memcpy(&SPRITE_PIXEL(anim->saved_chunk, 0, i), &GFX_PIXEL(ctx, anim->x, anim->y + i), anim->saved_chunk->width * 4);
                            }

                            anim->saved_chunk->alpha = SPRITE_ALPHA_SOLID;
                        }
                    }

                    gfx_createClip(ctx, anim->x, anim->y, anim->sprite->width, anim->sprite->height);

                    if (anim->saved_chunk) {
                        gfx_renderSprite(ctx, anim->saved_chunk, anim->x, anim->y);
                    }

                    if (anim->fn) anim->fn(ctx, anim);
                    if (anim->uframe) anim->uframe(ctx, anim);

                    anim->last_frame = now;
                    anim->frame++;
                } 

                if (anim->frame >= anim->total_frames) {
                    anim->state = GFX_ANIM_FINISHED;
                    if (anim->end) anim->end(ctx, anim);

                    node_t *old = an;
                    an = an->next;

                    list_delete(ctx->animations, old);

                    free(old);
                    
                    gfx_destroyAnimation(anim);
                    continue;
                }

            }

            an = an->next;
        }
    }
}


/**
 * @brief Fade in
 */
static void gfx_animationFadeInCallback(gfx_context_t *ctx, gfx_anim_t *anim) {
    gfx_fade_ctx_t *f = (gfx_fade_ctx_t*)anim->anim;
    gfx_renderSpriteAlpha(ctx, anim->sprite, anim->x, anim->y, f->start_alpha + anim->frame);
}

/**
 * @brief Fade out
 */
static void gfx_animationFadeOutCallback(gfx_context_t *ctx, gfx_anim_t *anim) {
    gfx_fade_ctx_t *f = (gfx_fade_ctx_t*)anim->anim;
    gfx_renderSpriteAlpha(ctx, anim->sprite, anim->x, anim->y, f->start_alpha - anim->frame);
}

/**
 * @brief Transition
 */
static void gfx_animationTransitionCallback(gfx_context_t *ctx, gfx_anim_t *anim) {
    gfx_renderSpriteAlpha(ctx, anim->anim, anim->x, anim->y, anim->frame);
    gfx_renderSpriteAlpha(ctx, anim->sprite, anim->x, anim->y, 255 - anim->frame);
}

/**
 * @brief Create a new animation
 * @param ctx The context to create the animation on
 * @param sprite The sprite to create the animation for
 * @param x X
 * @param y Y
 */
gfx_anim_t *gfx_createAnimation(struct gfx_context *ctx, sprite_t *sprite, int x, int y) {
    gfx_anim_t *a = malloc(sizeof(gfx_anim_t));
    memset(a, 0, sizeof(gfx_anim_t));

    a->sprite = sprite;
    a->x = x;
    a->y = y;
    a->state = GFX_ANIM_STOPPED;

    if (!ctx->animations) ctx->animations = list_create("gfx animations");
    list_append(ctx->animations, a);

    return a;
}

/**
 * @brief Create a fade in animation
 * @param ctx The context to create the animation for
 * @param sprite The sprite to do the fade in on
 * @param duration The duration to fade in at (in ms)
 * @param start_alpha Starting alpha
 * @param end_alpha Ending alpha
 * @param x X coordinate
 * @param y Y coordinate
 */
gfx_anim_t *gfx_animateFadeIn(struct gfx_context *ctx, sprite_t *sprite, int duration, uint8_t start_alpha, uint8_t end_alpha, int x, int y) {
    gfx_anim_t *a = gfx_createAnimation(ctx, sprite, x, y);

    gfx_fade_ctx_t *f = malloc(sizeof(gfx_fade_ctx_t));
    f->start_alpha = start_alpha;
    f->end_alpha = end_alpha;
    a->fn = gfx_animationFadeInCallback;
    a->anim = f;

    // Calculate duration
    a->total_frames = f->end_alpha - f->start_alpha;
    a->frame = 0;
    a->delay = duration * 1000 / a->total_frames; // !!!: what lol

    return a;
}

/**
 * @brief Create a fade out animation
 * @param ctx The context to create the animation for
 * @param sprite The sprite to do the fade out on
 * @param duration The duration to fade out at (in ms)
 * @param start_alpha Starting alpha
 * @param end_alpha Ending alpha
 * @param x X coordinate
 * @param y Y coordinate
 */
gfx_anim_t *gfx_animateFadeOut(struct gfx_context *ctx, sprite_t *sprite, int duration, uint8_t start_alpha, uint8_t end_alpha, int x, int y) {
    gfx_anim_t *a = gfx_createAnimation(ctx, sprite, x, y);

    gfx_fade_ctx_t *f = malloc(sizeof(gfx_fade_ctx_t));
    f->start_alpha = start_alpha;
    f->end_alpha = end_alpha;
    a->fn = gfx_animationFadeOutCallback;
    a->anim = f;

    // Calculate duration
    a->total_frames = f->start_alpha - f->end_alpha;
    a->frame = 0;
    a->delay = duration * 1000 / a->total_frames; // !!!: what lol

    return a;
}


/**
 * @brief Create a transition animation
 * @param ctx The context to create the animation for
 * @param sprite The sprite to start the transition
 * @param end_sprite The sprite to finish the transition on
 * @param duration The duration to transition for (in ms)
 * @param x X coordinate
 * @param y Y coordinate
 */
gfx_anim_t *gfx_animateTransition(struct gfx_context *ctx, sprite_t *sprite, sprite_t *end_sprite, int duration, int x, int y) {
    gfx_anim_t *a = gfx_createAnimation(ctx, sprite, x, y);
    a->fn = gfx_animationTransitionCallback;
    a->anim = end_sprite;

    // Calculate duration
    a->total_frames = 255;
    a->frame = 0;
    a->delay = duration * 1000 / a->total_frames; // !!!: what lol

    return a;
}

/**
 * @brief Start an animation
 * @param anim The animation to start
 */
void gfx_startAnimation(gfx_anim_t *anim) {
    anim->state = GFX_ANIM_RUNNING;
}

/**
 * @brief Stop an animation
 * @param anim The animation to stop
 */
void gfx_stopAnimation(gfx_anim_t *anim) {
    anim->state = GFX_ANIM_STOPPED;
}

/**
 * @brief Destroy animation
 * @param anim The animation to destroy
 */
void gfx_destroyAnimation(gfx_anim_t *anim) {
    if (anim->destroy) anim->destroy(anim);
    if (anim->saved_chunk) gfx_destroySprite(anim->saved_chunk);
    free(anim);
}