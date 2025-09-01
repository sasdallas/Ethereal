/**
 * @file userspace/lib/include/graphics/anim.h
 * @brief Graphics animations
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_ANIM_H
#define _GRAPHICS_ANIM_H

/**** INCLUDES ****/
#include <graphics/sprite.h>
#include <unistd.h>
#include <sys/time.h>

/**** DEFINITIONS ****/

#define GFX_ANIM_STOPPED        0
#define GFX_ANIM_RUNNING        1
#define GFX_ANIM_FINISHED       2

#define GFX_ANIM_DEFAULT        0
#define GFX_ANIM_FLAG_NO_SAVE   0x1

/**** TYPES ****/

struct gfx_anim;
struct gfx_context;

typedef void (*gfx_anim_callback_t)(struct gfx_context *ctx, struct gfx_anim *anim);
typedef void (*gfx_anim_destroy_t)(struct gfx_anim *anim);

typedef struct gfx_anim {
    sprite_t *sprite;               // Target sprite
    uint8_t flags;                  // Flags
    int x;                          // Starting X coordinate
    int y;                          // Starting Y coordinate
    int frame;                      // Current frame
    int total_frames;               // Total amount of frames
    uint64_t last_frame;            // Last frame time
    uint64_t delay;                 // Delay between frames
    uint8_t state;                  // Animation state

    sprite_t *saved_chunk;          // Saved chunk of the screen the animation encompasses.
                                    // Useful for contexts that don't rerender. Override by doing GFX_ANIM_FLAG_NO_SAVE 

    gfx_anim_callback_t fn;         // Animation function
    gfx_anim_callback_t uframe;     // User frame callback
    gfx_anim_callback_t start;      // Start of animation callback
    gfx_anim_callback_t end;        // End of animation callback
    gfx_anim_destroy_t destroy;     // Destructor

    void *anim;                     // Animation-specific (do not use if you plan on using builtin)
    void *user;                     // User-specific
} gfx_anim_t;

typedef struct gfx_fade_ctx {
    uint8_t start_alpha;
    uint8_t end_alpha;
} gfx_fade_ctx_t;



/**** FUNCTIONS ****/

/**
 * @brief Tick animations
 * @param ctx The context to tick animations on
 */
void gfx_tickAnimations(struct gfx_context *ctx);

/**
 * @brief Create a new animation
 * @param ctx The context to create the animation on
 * @param sprite The sprite to create the animation for
 * @param x X
 * @param y Y
 */
gfx_anim_t *gfx_createAnimation(struct gfx_context *ctx, sprite_t *sprite, int x, int y);

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
gfx_anim_t *gfx_animateFadeIn(struct gfx_context *ctx, sprite_t *sprite, int duration, uint8_t start_alpha, uint8_t end_alpha, int x, int y);


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
gfx_anim_t *gfx_animateFadeOut(struct gfx_context *ctx, sprite_t *sprite, int duration, uint8_t start_alpha, uint8_t end_alpha, int x, int y);

/**
 * @brief Create a transition animation
 * @param ctx The context to create the animation for
 * @param sprite The sprite to start the transition
 * @param end_sprite The sprite to finish the transition on
 * @param duration The duration to transition for (in ms)
 * @param x X coordinate
 * @param y Y coordinate
 */
gfx_anim_t *gfx_animateTransition(struct gfx_context *ctx, sprite_t *sprite, sprite_t *end_sprite, int duration, int x, int y);

/**
 * @brief Start an animation
 * @param anim The animation to start
 */
void gfx_startAnimation(gfx_anim_t *anim);

/**
 * @brief Stop an animation
 * @param anim The animation to stop
 */
void gfx_stopAnimation(gfx_anim_t *anim);


/**
 * @brief Destroy animation
 * @param anim The animation to destroy
 */
void gfx_destroyAnimation(gfx_anim_t *anim);

#endif